#include "projectmanager.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include "componentmanager.h"
#include "projectsetting.h"
#include <QSaveFile>
#include <QFileInfo>
#include <QProgressDialog>
#include <QEventLoop>
#include "projectworker.h"
#include <QThread>
#include <QSet>
#include <algorithm>
#include "backendworker.h"

namespace {
bool isStringArray(const QJsonValue& value) {
    if (!value.isArray()) return false;
    for (const auto& item : value.toArray()) {
        if (!item.isString()) return false;
    }
    return true;
}

bool isValidMemberSection(const QJsonValue& value) {
    if (!value.isObject()) return false;
    const auto section = value.toObject();
    if (!isStringArray(section["allProperties"]) || !section["visibility"].isObject()
        || !section["list"].isArray()) return false;
    QSet<QUuid> ids;
    for (const auto& member : section["list"].toArray()) {
        if (!member.isObject()) return false;
        const auto object = member.toObject();
        const QUuid id(object["id"].toString());
        if (id.isNull() || ids.contains(id) || !object["properties"].isObject()
            || !isStringArray(object["scheduleDates"]) || !isStringArray(object["scheduleTimes"])
            || !object["scheduleMap"].isObject()) return false;
        ids.insert(id);
        const auto schedule = object["scheduleMap"].toObject();
        for (auto it = schedule.begin(); it != schedule.end(); ++it) {
            if (!it.value().isObject()) return false;
            const auto times = it.value().toObject();
            for (auto time = times.begin(); time != times.end(); ++time) {
                if (!time.value().isBool()) return false;
            }
        }
    }
    return true;
}

bool isValidProject(const QJsonObject& root) {
    if ((root.contains("version") && !root["version"].isString())
        || (root.contains("projectName") && !root["projectName"].isString())
        || !isValidMemberSection(root["memberData"]) || !root["editorConfig"].isObject()) return false;
    const auto editor = root["editorConfig"].toObject();
    const int mode = editor["currentMode"].toInt(-1);
    if ((mode != EditorDutyModel::WeekMode && mode != EditorDutyModel::DateMode)
        || !isStringArray(editor["timeRows"]) || !isStringArray(editor["weekColumns"])
        || !isStringArray(editor["dateColumns"]) || !editor["assignments"].isObject()) return false;
    const auto assignments = editor["assignments"].toObject();
    for (auto it = assignments.begin(); it != assignments.end(); ++it) {
        if (!isStringArray(it.value())) return false;
        for (const auto& id : it.value().toArray()) {
            if (QUuid(id.toString()).isNull()) return false;
        }
    }
    return true;
}

void clearUndoHistory() {
    auto* components = ComponentManager::instance();
    components->getComponent<MemberInfoComponent>()->getController()->getUndoStack()->clear();
    components->getComponent<EditorDutyComponent>()->getController()->getUndoStack()->clear();
}
}

ProjectManager* ProjectManager::instance() {
    static ProjectManager instance;
    return &instance;
}

ProjectManager::ProjectManager(QObject *parent)
	: QObject(parent)
{
    version = "1.0";
    auto* mModel = ComponentManager::instance()->getComponent<MemberInfoComponent>()->getModel();
    auto* eModel = ComponentManager::instance()->getComponent<EditorDutyComponent>()->getModel();
    auto* sModel = ComponentManager::instance()->getComponent<ScheduleComponent>()->getModel();

    connect(mModel, &QAbstractItemModel::dataChanged, this, [this]() {
        setDirty(true);
        });
    connect(eModel, &EditorDutyModel::assignmentsChanged, this, [this]() {
        setDirty(true);
        });

    connect(sModel, &QAbstractItemModel::dataChanged, this, [this]() {
        setDirty(true);
        });
    connect(sModel, &QAbstractItemModel::rowsInserted, this, [this]() {
        setDirty(true);
        });

    // Structural edits and hidden fields are saved too, even when no visible cell changes.
    for (auto* model : { static_cast<QAbstractItemModel*>(mModel),
                         static_cast<QAbstractItemModel*>(eModel) }) {
        connect(model, &QAbstractItemModel::modelReset, this, [this]() { setDirty(true); });
        connect(model, &QAbstractItemModel::rowsInserted, this, [this]() { setDirty(true); });
        connect(model, &QAbstractItemModel::rowsRemoved, this, [this]() { setDirty(true); });
        connect(model, &QAbstractItemModel::columnsInserted, this, [this]() { setDirty(true); });
        connect(model, &QAbstractItemModel::columnsRemoved, this, [this]() { setDirty(true); });
    }
    connect(mModel, &MemberInfoModel::propertyListChanged, this, [this]() { setDirty(true); });
    connect(sModel, &QAbstractItemModel::rowsRemoved, this, [this]() { setDirty(true); });
    connect(sModel, &QAbstractItemModel::columnsInserted, this, [this]() { setDirty(true); });
    connect(sModel, &QAbstractItemModel::columnsRemoved, this, [this]() { setDirty(true); });
    connect(sModel, &QAbstractItemModel::headerDataChanged, this, [this]() { setDirty(true); });
    connect(ComponentManager::instance()->getComponent<MemberInfoComponent>()->getController()->getUndoStack(),
        &QUndoStack::indexChanged, this, [this]() { setDirty(true); });
    connect(ComponentManager::instance()->getComponent<EditorDutyComponent>()->getController()->getUndoStack(),
        &QUndoStack::indexChanged, this, [this]() { setDirty(true); });
}

ProjectManager::~ProjectManager() {
    const auto threads = findChildren<QThread*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* thread : threads) {
        thread->quit();
        thread->wait();
    }
}

QJsonObject ProjectManager::serializeData() {
    QJsonObject root;
    root["version"] = version;
    root["projectName"] = ProjectSetting::instance()->_projectName;
    root["toleranceValue"] = ProjectSetting::instance()->_toleranceValue;

    QJsonObject dateSearchSection;
    dateSearchSection["enabled"] = ProjectSetting::instance()->_enableSerchDateRange;
    dateSearchSection["startDate"] = ProjectSetting::instance()->_searchStartDate.toString(Qt::ISODate);
    dateSearchSection["endDate"] = ProjectSetting::instance()->_searchEndDate.toString(Qt::ISODate);
    root["dateSearchConfig"] = dateSearchSection;

    auto* mModel = ComponentManager::instance()->getComponent<MemberInfoComponent>()->getModel();
    auto* eModel = ComponentManager::instance()->getComponent<EditorDutyComponent>()->getModel();

    QJsonObject memberSection;
    memberSection["allProperties"] = QJsonArray::fromStringList(mModel->getProperties());

    QJsonObject visibilityObj;
    QVariantMap showProps = mModel->getShowProperties();
    for (auto it = showProps.begin(); it != showProps.end(); ++it) {
        visibilityObj[it.key()] = it.value().toBool();
    }
    memberSection["visibility"] = visibilityObj;

    QJsonArray memberArray;
    for (int i = 0; i < mModel->rowCount(); ++i) {
        auto* data = mModel->getMemberDataPtr(i);
        if (!data) continue;

        QJsonObject mObj;
        mObj["id"] = data->id.toString();
        mObj["note"] = data->note;
        mObj["scheduleDates"] = QJsonArray::fromStringList(data->scheduleDates);
        mObj["scheduleTimes"] = QJsonArray::fromStringList(data->scheduleTimes);

        QJsonObject propObj;
        for (auto it = data->properties.begin(); it != data->properties.end(); ++it) {
            propObj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        mObj["properties"] = propObj;

        QJsonObject scheduleObj;
        for (auto dateIt = data->scheduleMap.begin(); dateIt != data->scheduleMap.end(); ++dateIt) {
            QJsonObject timeObj;
            for (auto timeIt = dateIt.value().begin(); timeIt != dateIt.value().end(); ++timeIt) {
                timeObj[timeIt.key()] = timeIt.value();
            }
            scheduleObj[dateIt.key()] = timeObj;
        }
        mObj["scheduleMap"] = scheduleObj;
        mObj["enableScheduling"] = data->enableScheduling;
        mObj["scheduleFileSource"] = data->scheduleFileSource;

        memberArray.append(mObj);
    }
    memberSection["list"] = memberArray;
    root["memberData"] = memberSection;

    QJsonObject editorSection;
    editorSection["currentMode"] = static_cast<int>(eModel->getColumnMode());
    editorSection["timeRows"] = QJsonArray::fromStringList(*(eModel->getTimesRow()));
    editorSection["weekColumns"] = QJsonArray::fromStringList(eModel->getWeekColumns());
    editorSection["dateColumns"] = QJsonArray::fromStringList(eModel->getDateColumns());

    QJsonObject assignmentsObj;
    const QHash<QString, QList<QUuid>>& idHash = eModel->getSelectedIdsMap();

    QHash<QString, QList<QUuid>>::const_iterator it = idHash.constBegin();
    while (it != idHash.constEnd()) {
        QJsonArray idArray;
        for (const QUuid& id : it.value()) {
            idArray.append(id.toString());
        }
        assignmentsObj.insert(it.key(), idArray);
        ++it;
    }
    editorSection["assignments"] = assignmentsObj;

    root["editorConfig"] = editorSection;

    return root;
}

void ProjectManager::deserializeData(const QJsonObject& root) {
    auto* eModel = ComponentManager::instance()->getComponent<EditorDutyComponent>()->getModel();

    ProjectSetting::instance()->_projectName = root["projectName"].toString();
    ProjectSetting::instance()->_toleranceValue = qBound(0, root["toleranceValue"].toInt(0), 5);

    ProjectSetting::instance()->_enableSerchDateRange = false;
    ProjectSetting::instance()->_searchStartDate = QDate::currentDate();
    ProjectSetting::instance()->_searchEndDate = QDate::currentDate().addMonths(3);
    if (root.contains("dateSearchConfig")) {
        QJsonObject dateObj = root["dateSearchConfig"].toObject();
        QDate sDate = QDate::fromString(dateObj["startDate"].toString(), Qt::ISODate);
        QDate eDate = QDate::fromString(dateObj["endDate"].toString(), Qt::ISODate);

        if (sDate.isValid() && eDate.isValid() && sDate <= eDate) {
            ProjectSetting::instance()->_enableSerchDateRange = dateObj["enabled"].toBool();
            ProjectSetting::instance()->_searchStartDate = sDate;
            ProjectSetting::instance()->_searchEndDate = eDate;
        }
    }
    QJsonObject memberSection = root["memberData"].toObject();
    deserializeMemberData(memberSection);

    QJsonObject editorSection = root["editorConfig"].toObject();

    eModel->beginResetModel();
    eModel->_currentMode = static_cast<EditorDutyModel::ColumnMode>(editorSection["currentMode"].toInt());//恢复枚举模式

    eModel->_timeRows = editorSection["timeRows"].toVariant().toStringList();
    eModel->_weekColumns = editorSection["weekColumns"].toVariant().toStringList();
    eModel->_dateColumns = editorSection["dateColumns"].toVariant().toStringList();

    QJsonObject assignmentsObj = editorSection["assignments"].toObject();
    QHash<QString, QList<QUuid>> restoredHash;
    for (auto it = assignmentsObj.begin(); it != assignmentsObj.end(); ++it) {
        QJsonArray idArray = it.value().toArray();
        QList<QUuid> ids;
        for (const QJsonValue& v : idArray) {
            QUuid id = QUuid::fromString(v.toString());
            if (!id.isNull()) ids.append(id);
        }
        restoredHash.insert(it.key(), ids);
    }
    // Loading replaces both modes; setSelectedIdsMap deliberately merges the inactive mode.
    eModel->_selectedIdsMap = restoredHash;
    eModel->_cellDataMap.clear();
    eModel->_selectedMembersMap.clear();

    eModel->endResetModel();
    
}

void ProjectManager::deserializeMemberData(const QJsonObject& memberSection) {
    auto* mModel = ComponentManager::instance()->getComponent<MemberInfoComponent>()->getModel();

    mModel->beginResetModel();

    mModel->_allProperties = memberSection["allProperties"].toVariant().toStringList();
    if (!mModel->_allProperties.contains("姓名")) mModel->_allProperties.prepend("姓名");
    mModel->_properties = memberSection["visibility"].toObject().toVariantMap();
    mModel->_properties["姓名"] = true;
    mModel->_visibleColumns.clear();
    mModel->_visibleColumns << "姓名";
    for (auto it = mModel->_properties.begin(); it != mModel->_properties.end(); ++it) {
        if (it.key() != "姓名" && it.value().toBool()) {
            mModel->_visibleColumns << it.key();
        }
    }

    mModel->_members.clear();
    QJsonArray memberArray = memberSection["list"].toArray();
    for (const auto& mVal : memberArray) {
        QJsonObject mObj = mVal.toObject();
        MemberInfoModel::MemberData data;

        data.id = QUuid::fromString(mObj["id"].toString());
        data.note = mObj["note"].toString();
        data.properties = mObj["properties"].toObject().toVariantMap();
        data.scheduleDates = mObj["scheduleDates"].toVariant().toStringList();
        data.scheduleTimes = mObj["scheduleTimes"].toVariant().toStringList();

        data.enableScheduling = mObj.contains("enableScheduling") ? mObj["enableScheduling"].toBool() : true;
        data.scheduleFileSource = mObj["scheduleFileSource"].toString();

        QJsonObject scheduleObj = mObj["scheduleMap"].toObject();
        for (auto dateKey : scheduleObj.keys()) {
            QJsonObject timeObj = scheduleObj[dateKey].toObject();
            QMap<QString, bool> timeMap;
            for (auto timeKey : timeObj.keys()) {
                timeMap.insert(timeKey, timeObj[timeKey].toBool());
            }
            data.scheduleMap.insert(dateKey, timeMap);
        }
        mModel->_members.append(data);
    }
    mModel->endResetModel();
    Q_EMIT mModel->propertyListChanged();
}

void ProjectManager::exportMembersAs(const QString& filePath) {
    if (filePath.isEmpty()) return;

    QJsonObject root;
    root["version"] = version;
    root["type"] = "MemberExport";

    auto* mModel = ComponentManager::instance()->getComponent<MemberInfoComponent>()->getModel();

    QJsonObject memberSection;
    memberSection["allProperties"] = QJsonArray::fromStringList(mModel->getProperties());

    QJsonObject visibilityObj;
    QVariantMap showProps = mModel->getShowProperties();
    for (auto it = showProps.begin(); it != showProps.end(); ++it) {
        visibilityObj[it.key()] = it.value().toBool();
    }
    memberSection["visibility"] = visibilityObj;

    QJsonArray memberArray;
    for (int i = 0; i < mModel->rowCount(); ++i) {
        auto* data = mModel->getMemberDataPtr(i);
        if (!data) continue;

        QJsonObject mObj;
        mObj["id"] = data->id.toString();
        mObj["note"] = data->note;
        mObj["scheduleDates"] = QJsonArray::fromStringList(data->scheduleDates);
        mObj["scheduleTimes"] = QJsonArray::fromStringList(data->scheduleTimes);

        QJsonObject propObj;
        for (auto it = data->properties.begin(); it != data->properties.end(); ++it) {
            propObj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        mObj["properties"] = propObj;

        QJsonObject scheduleObj;
        for (auto dateIt = data->scheduleMap.begin(); dateIt != data->scheduleMap.end(); ++dateIt) {
            QJsonObject timeObj;
            for (auto timeIt = dateIt.value().begin(); timeIt != dateIt.value().end(); ++timeIt) {
                timeObj[timeIt.key()] = timeIt.value();
            }
            scheduleObj[dateIt.key()] = timeObj;
        }
        mObj["scheduleMap"] = scheduleObj;
        mObj["enableScheduling"] = data->enableScheduling;
        mObj["scheduleFileSource"] = data->scheduleFileSource;
        memberArray.append(mObj);
    }
    memberSection["list"] = memberArray;
    root["memberData"] = memberSection;

    QThread* thread = new QThread(this);
    ProjectWorker* worker = new ProjectWorker;
    worker->moveToThread(thread);
    connect(worker, &ProjectWorker::saveDone, thread, &QThread::quit, Qt::DirectConnection);
    connect(worker, &ProjectWorker::loadDone, thread, &QThread::quit, Qt::DirectConnection);
    connect(worker, &ProjectWorker::error, thread, &QThread::quit, Qt::DirectConnection);

    connect(thread, &QThread::started, worker, [=]() { worker->doSave(filePath, root); });
    connect(worker, &ProjectWorker::saveDone, this, [this]() {
        Q_EMIT exportFinished(true);
        });
    connect(worker, &ProjectWorker::error, this, [this](QString msg) {
        Q_EMIT operationError(msg);
        Q_EMIT exportFinished(false);
        });
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void ProjectManager::importMembersAsync(const QString& filePath) {
    if (filePath.isEmpty()) return;

    QThread* thread = new QThread(this);
    ProjectWorker* worker = new ProjectWorker;
    worker->moveToThread(thread);
    connect(worker, &ProjectWorker::saveDone, thread, &QThread::quit, Qt::DirectConnection);
    connect(worker, &ProjectWorker::loadDone, thread, &QThread::quit, Qt::DirectConnection);
    connect(worker, &ProjectWorker::error, thread, &QThread::quit, Qt::DirectConnection);

    connect(thread, &QThread::started, worker, [=]() {
        worker->doLoad(filePath);
        });
    connect(worker, &ProjectWorker::loadDone, this, [this](const QJsonObject& data) {
        if (data["type"].toString() != "MemberExport" || !isValidMemberSection(data["memberData"])) {
            Q_EMIT operationError("导入失败：该文件不是有效的成员导出文件(.rmem)。");
            Q_EMIT importMembersFinished(false);
        }
        else {
            clearUndoHistory();
            this->deserializeMemberData(data["memberData"].toObject());
            auto* editor = ComponentManager::instance()->getComponent<EditorDutyComponent>()->getModel();
            QSet<QUuid> validIds;
            auto* members = ComponentManager::instance()->getComponent<MemberInfoComponent>()->getModel();
            for (int row = 0; row < members->rowCount(); ++row) validIds.insert(members->getMemberDataPtr(row)->id);
            for (auto it = editor->_selectedIdsMap.begin(); it != editor->_selectedIdsMap.end(); ++it) {
                auto& ids = it.value();
                ids.erase(std::remove_if(ids.begin(), ids.end(), [&validIds](const QUuid& id) {
                    return !validIds.contains(id);
                }), ids.end());
            }
            editor->_cellDataMap.clear();
            editor->_selectedMembersMap.clear();
            editor->refreshView();
            this->setDirty(true);
            Q_EMIT importMembersFinished(true);
        }
        });

    connect(worker, &ProjectWorker::error, this, [this](QString msg) {
        Q_EMIT operationError("文件读取错误: " + msg);
        Q_EMIT importMembersFinished(false);
        });

    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}


QString ProjectManager::currentFilePath() const { 
	return _currentFilePath; 
}

bool ProjectManager::newProject(const QString& name) {
    auto* mModel = ComponentManager::instance()->getComponent<MemberInfoComponent>()->getModel();
    auto* eModel = ComponentManager::instance()->getComponent<EditorDutyComponent>()->getModel();

    ++_projectGeneration;
    clearUndoHistory();
    ProjectSetting::instance()->reset();//重置全局配置
    ProjectSetting::instance()->_projectName = name;

    mModel->reset();
    eModel->reset();
    eModel->setColumnMode(EditorDutyModel::WeekMode);

    //重置状态机
    _currentFilePath.clear();
    setDirty(false);

    Q_EMIT ProjectSetting::instance()->projectNameChanged();
    Q_EMIT projectLoaded();
    Q_EMIT projectStatusChanged();

    return true;
}

ProjectManager::SaveStatus ProjectManager::checkSaveStatus() const {
    return _currentFilePath.isEmpty() ? SaveStatus::NeedPath : SaveStatus::Success;
}

void ProjectManager::openProject(const QString& filePath) {
    if (filePath.isEmpty()) return;
    QThread* thread = new QThread(this);
    ProjectWorker* worker = new ProjectWorker;
    worker->moveToThread(thread);
    connect(worker, &ProjectWorker::saveDone, thread, &QThread::quit, Qt::DirectConnection);
    connect(worker, &ProjectWorker::loadDone, thread, &QThread::quit, Qt::DirectConnection);
    connect(worker, &ProjectWorker::error, thread, &QThread::quit, Qt::DirectConnection);

    connect(thread, &QThread::started, worker, [=]() { worker->doLoad(filePath); });
    connect(worker, &ProjectWorker::loadDone, this, [this, filePath](const QJsonObject& data) {
        if (!isValidProject(data)) {
            Q_EMIT operationError("文件结构无效，不是有效的 RabbitRoll 项目文件，当前项目未更改。");
            return;
        }
        ++_projectGeneration;
        clearUndoHistory();
        this->deserializeData(data); 
        QFileInfo fileInfo(filePath);
        ProjectSetting::instance()->_projectName = fileInfo.completeBaseName();

        this->_currentFilePath = filePath;
        this->setDirty(false);
        Q_EMIT projectLoaded();
        });

    connect(worker, &ProjectWorker::error, this, [this](QString msg) {
        Q_EMIT operationError(msg);
        });
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void ProjectManager::saveProject() {
    if (_currentFilePath.isEmpty()) {
        Q_EMIT operationError("文件路径为空，请选择另存为。");
        Q_EMIT saveFinished(false);
        return;
    }
    saveProjectAs(_currentFilePath);
}


void ProjectManager::saveProjectAs(const QString& filePath) {
    if (filePath.isEmpty()) {
        Q_EMIT saveFinished(false);
        return;
    }

    QJsonObject root = serializeData();
    const QString savedName = QFileInfo(filePath).completeBaseName();
    root["projectName"] = savedName;
    const auto savedRevision = _changeRevision;
    const auto savedGeneration = _projectGeneration;

    QThread* thread = new QThread(this);
    ProjectWorker* worker = new ProjectWorker;
    worker->moveToThread(thread);
    connect(worker, &ProjectWorker::saveDone, thread, &QThread::quit, Qt::DirectConnection);
    connect(worker, &ProjectWorker::loadDone, thread, &QThread::quit, Qt::DirectConnection);
    connect(worker, &ProjectWorker::error, thread, &QThread::quit, Qt::DirectConnection);

    connect(thread, &QThread::started, worker, [=]() {
        worker->doSave(filePath, root); 
        });
    connect(worker, &ProjectWorker::saveDone, this, [this, filePath, savedName, savedRevision, savedGeneration]() {
        if (_projectGeneration == savedGeneration) {
            this->_currentFilePath = filePath;
            ProjectSetting::instance()->_projectName = savedName;
            if (_changeRevision == savedRevision) this->setDirty(false);
            Q_EMIT projectStatusChanged();
        }
        Q_EMIT saveFinished(true);
        });
    connect(worker, &ProjectWorker::error, this, [this](QString msg) {
        Q_EMIT operationError(msg);
        Q_EMIT saveFinished(false);
        });
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void ProjectManager::setDirty(bool dirty) {
    if (dirty) ++_changeRevision;
    if (_isDirty == dirty) return;

    _isDirty = dirty;

    Q_EMIT projectStatusChanged();
}

bool ProjectManager::isDirty() const { 
    return _isDirty; 
}

void ProjectManager::importScheduleFromFile(const QString& filePath, const QDate& startDate) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Q_EMIT operationError("无法打开课表文件，请检查权限。");
        Q_EMIT scheduleImportFinished(false, QJsonObject());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    QJsonObject root = doc.object();

    QString dataType = root["dataType"].toString();
    if (root["app"].toString() != "rabbitroll" || (dataType != "course_schedule" && dataType != "schedule")) {
        Q_EMIT operationError("该文件不是有效的时间表格式。");
        Q_EMIT scheduleImportFinished(false, QJsonObject());
        return;
    }

    BackendWorker* worker = BackendWorker::instance();

    connect(worker, &BackendWorker::scheduleImportFinished, this, [this](const QJsonObject& result) {
        Q_EMIT scheduleImportFinished(true, result);
        }, Qt::SingleShotConnection);

    QMetaObject::invokeMethod(worker, [worker, root, startDate]() {
        worker->scheduleImport(root, startDate);
    }, Qt::QueuedConnection);
}

void ProjectManager::exportScheduleAs(const QString& filePath, const QJsonObject& scheduleData) {
    if (filePath.isEmpty()) return;

    QJsonObject root;
    root["app"] = "rabbitroll";
    root["dataType"] = "schedule";
    root["version"] = version;
    root["exportTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["scheduleData"] = scheduleData;

    QThread* thread = new QThread(this);
    ProjectWorker* worker = new ProjectWorker;
    worker->moveToThread(thread);
    connect(worker, &ProjectWorker::saveDone, thread, &QThread::quit, Qt::DirectConnection);
    connect(worker, &ProjectWorker::loadDone, thread, &QThread::quit, Qt::DirectConnection);
    connect(worker, &ProjectWorker::error, thread, &QThread::quit, Qt::DirectConnection);

    connect(thread, &QThread::started, worker, [=]() {
        worker->doSave(filePath, root);
        });
    connect(worker, &ProjectWorker::saveDone, this, [this]() {
        Q_EMIT scheduleExportFinished(true);
        });
    connect(worker, &ProjectWorker::error, this, [this](QString msg) {
        Q_EMIT operationError(msg);
        Q_EMIT scheduleExportFinished(false);
        });

    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}
