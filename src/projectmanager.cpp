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
#include "backendworker.h"

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
    connect(mModel, &QAbstractItemModel::rowsInserted, this, [this]() {
        setDirty(true);
        });
    connect(mModel, &QAbstractItemModel::rowsRemoved, this, [this]() {
        setDirty(true);
        });

    connect(eModel, &QAbstractItemModel::dataChanged, this, [this]() {
        setDirty(true);
        });

    connect(sModel, &QAbstractItemModel::dataChanged, this, [this]() {
        setDirty(true);
        });
    connect(sModel, &QAbstractItemModel::rowsInserted, this, [this]() {
        setDirty(true);
        });
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
    QString version = root["version"].toString();
    ProjectSetting::instance()->_toleranceValue = root["toleranceValue"].toInt(0);

    if (root.contains("dateSearchConfig")) {
        QJsonObject dateObj = root["dateSearchConfig"].toObject();
        ProjectSetting::instance()->_enableSerchDateRange = dateObj["enabled"].toBool();
        QDate sDate = QDate::fromString(dateObj["startDate"].toString(), Qt::ISODate);
        QDate eDate = QDate::fromString(dateObj["endDate"].toString(), Qt::ISODate);

        if (sDate.isValid() && eDate.isValid()) {
            ProjectSetting::instance()->_searchStartDate = sDate;
            ProjectSetting::instance()->_searchEndDate = eDate;
        }
    }
    else {
        //如果旧文件中没有这个字段就执行重置
        ProjectSetting::instance()->_enableSerchDateRange = false;
        ProjectSetting::instance()->_searchStartDate = QDate::currentDate();
        ProjectSetting::instance()->_searchEndDate = QDate::currentDate().addMonths(3);
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
    eModel->setSelectedIdsMap(restoredHash);

    eModel->endResetModel();
    
}

void ProjectManager::deserializeMemberData(const QJsonObject& memberSection) {
    auto* mModel = ComponentManager::instance()->getComponent<MemberInfoComponent>()->getModel();

    mModel->beginResetModel();

    mModel->_allProperties = memberSection["allProperties"].toVariant().toStringList();
    mModel->_properties = memberSection["visibility"].toObject().toVariantMap();
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
        memberArray.append(mObj);
    }
    memberSection["list"] = memberArray;
    root["memberData"] = memberSection;

    QThread* thread = new QThread;
    ProjectWorker* worker = new ProjectWorker;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [=]() { worker->doSave(filePath, root); });
    connect(worker, &ProjectWorker::saveDone, this, [this, thread]() {
        Q_EMIT exportFinished(true);
        thread->quit();
        });
    connect(worker, &ProjectWorker::error, this, [this, thread](QString msg) {
        Q_EMIT operationError(msg);
        Q_EMIT exportFinished(false);
        thread->quit();
        });
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void ProjectManager::importMembersAsync(const QString& filePath) {
    if (filePath.isEmpty()) return;

    QThread* thread = new QThread;
    ProjectWorker* worker = new ProjectWorker;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [=]() {
        worker->doLoad(filePath);
        });
    connect(worker, &ProjectWorker::loadDone, this, [this, thread](const QJsonObject& data) {
        if (data["type"].toString() != "MemberExport") {
            Q_EMIT operationError("导入失败：该文件不是有效的成员导出文件(.rmem)。");
            Q_EMIT importMembersFinished(false);
        }
        else {
            this->deserializeMemberData(data["memberData"].toObject());
            this->setDirty(true);
            Q_EMIT importMembersFinished(true);
        }
        thread->quit();
        });

    connect(worker, &ProjectWorker::error, this, [this, thread](QString msg) {
        Q_EMIT operationError("文件读取错误: " + msg);
        Q_EMIT importMembersFinished(false);
        thread->quit();
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

	ProjectSetting::instance()->reset();//重置全局配置
    ProjectSetting::instance()->_projectName = name;

    mModel->reset();
    eModel->reset();

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
    QThread* thread = new QThread;
    ProjectWorker* worker = new ProjectWorker;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [=]() { worker->doLoad(filePath); });
    connect(worker, &ProjectWorker::loadDone, this, [this, thread, filePath](const QJsonObject& data) {
        this->deserializeData(data); 
        QFileInfo fileInfo(filePath);
        ProjectSetting::instance()->_projectName = fileInfo.completeBaseName();

        this->_currentFilePath = filePath;
        this->setDirty(false);
        Q_EMIT projectLoaded();
        thread->quit();
        });

    connect(worker, &ProjectWorker::error, this, [this, thread](QString msg) {
        Q_EMIT operationError(msg);
        thread->quit();
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
    if (filePath.isEmpty()) return;

    QJsonObject root = serializeData();

    QThread* thread = new QThread;
    ProjectWorker* worker = new ProjectWorker;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [=]() {
        worker->doSave(filePath, root); 
        });
    connect(worker, &ProjectWorker::saveDone, this, [this, thread, filePath]() {
        this->_currentFilePath = filePath;
        this->setDirty(false);
        Q_EMIT projectStatusChanged();
        Q_EMIT saveFinished(true);
        thread->quit();
        });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(worker, &ProjectWorker::error, this, [this, thread](QString msg) {
        Q_EMIT operationError(msg);
        Q_EMIT saveFinished(false);
        thread->quit();
        });
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void ProjectManager::setDirty(bool dirty) {
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

    QThread* thread = new QThread;
    ProjectWorker* worker = new ProjectWorker;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, [=]() {
        worker->doSave(filePath, root);
        });
    connect(worker, &ProjectWorker::saveDone, this, [this, thread]() {
        Q_EMIT scheduleExportFinished(true);
        thread->quit();
        });
    connect(worker, &ProjectWorker::error, this, [this, thread](QString msg) {
        Q_EMIT operationError(msg);
        Q_EMIT scheduleExportFinished(false);
        thread->quit();
        });

    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}
