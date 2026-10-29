#include "memberinfocontroller.h"
#include "memberinfomodel.h"
#include "memberinfoview.h"
#include <QLayout>
#include <QDate>
#include "backendworker.h"
#include <QHeaderView>
#include "componentmanager.h"
#include "scriptapi.h"
#include "undocommands.h"

//开发过程中更换过一次架构，导致存在一些数据访问仍然是直接访问Model的情况。
//怕改了有BUG，暂时没动。

MemberInfoController::MemberInfoController(MemberInfoModel* model, MemberInfoView* view, QObject* parent)
    : QObject(parent), _model(model), _view(view)
{
    _undoStack = new QUndoStack(this);
    connect(_view, &MemberInfoView::memberSelected, this, [=](const QVariantMap& details, int row) {
        Q_EMIT memberSelected(details, row);
        });

    connect(_model, &MemberInfoModel::propertyListChanged, this, [=]() {
        Q_EMIT propertyListChanged();
        });

    connect(BackendWorker::instance(), &BackendWorker::batchMembersFinished, this, &MemberInfoController::onBatchMembersProcessed);

    /*connect(_model, &MemberInfoModel::modelIniRest, _view, [=]() {
        if (_model->columnCount() > 0) {
            _view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
            _view->setColumnWidth(0, 50);
        }
        });*/

    connect(BackendWorker::instance(), &BackendWorker::batchMembersWithFormatFinished, this, &MemberInfoController::onBatchMembersWithFormatProcessed);

	connect(_view, &MemberInfoView::deleteSelectedRows, this, &MemberInfoController::removeSelectedMembers);

    connect(BackendWorker::instance(), &BackendWorker::searchMemberFinished, this, &MemberInfoController::searchDataReady);

    connect(_model, &QAbstractTableModel::rowsInserted, this, &MemberInfoController::requestSearchSuggestionsUpdate);
    connect(_model, &QAbstractTableModel::rowsRemoved, this, &MemberInfoController::requestSearchSuggestionsUpdate);
    connect(_model, &QAbstractTableModel::modelReset, this, &MemberInfoController::requestSearchSuggestionsUpdate);
    connect(_model, &QAbstractTableModel::dataChanged, this, [=](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles) {
        requestSearchSuggestionsUpdate();
        });
}

MemberInfoController::~MemberInfoController()
{
}


void MemberInfoController::setupView(QLayout* layout) {
    if (layout && _view) {
        layout->addWidget(_view);
    }
}

void MemberInfoController::addMember(const QString& name) {
    auto newMember = _model->createDefaultMemberData(name);
    QVariantMap map; map["row"] = _model->rowCount(); map["data"] = QVariant::fromValue(newMember);
    _undoStack->push(new MemberAddCommand(this, _model->rowCount(), { map }));
}

void MemberInfoController::onBatchMembersProcessed(const QStringList& names) {
    if (names.isEmpty()) return;
    auto newMembers = _model->createBatchMembersData(names);
    QList<QVariantMap> mapList;
    for (int i = 0; i < newMembers.size(); ++i) {
        QVariantMap map; map["row"] = _model->rowCount() + i; map["data"] = QVariant::fromValue(newMembers[i]);
        mapList.append(map);
    }
    _undoStack->push(new MemberAddCommand(this, _model->rowCount(), mapList));
    Q_EMIT batchWorkFinished();
}

void MemberInfoController::onBatchMembersWithFormatProcessed(const QList<QVariantMap>& membersData, const QStringList& format) {
    bool hasNewProperty = false;
    _undoStack->beginMacro("从模板批量导入成员");
    for (const QString& prop : format) {
        if (!_model->getProperties().contains(prop)) {
            _undoStack->push(new MemberAddPropertyCommand(this, prop));
            hasNewProperty = true;
        }
    }

    auto newMembers = _model->createBatchMembersDataWithFormat(membersData);
    QList<QVariantMap> mapList;
    for (int i = 0; i < newMembers.size(); ++i) {
        QVariantMap map;
        map["row"] = _model->rowCount() + i;
        map["data"] = QVariant::fromValue(newMembers[i]);
        mapList.append(map);
    }

    _undoStack->push(new MemberAddCommand(this, _model->rowCount(), mapList));
    _undoStack->endMacro();

    Q_EMIT batchWorkFinished();
    if (hasNewProperty) {
        Q_EMIT propertyListChanged();
    }
}

void MemberInfoController::removeSelectedMembers() {
    QModelIndexList selectedRows = _view->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) return;

    QList<QVariantMap> removedData;
    for (const QModelIndex& idx : selectedRows) {
        MemberInfoModel::MemberData* ptr = _model->getMemberDataPtr(idx.row());
        if (ptr) {
            QVariantMap map; map["row"] = idx.row(); map["data"] = QVariant::fromValue(*ptr);
            removedData.append(map);
        }
    }
    _undoStack->push(new MemberRemoveCommand(this, removedData));
    _view->clearSelection();
}

void MemberInfoController::updateMemberProperty(int row, const QString& attrName, const QVariant& newValue) {
    QVariant oldValue = _model->getMemberAllData(row).value(attrName);
    if (oldValue == newValue) return;
    _undoStack->push(new MemberUpdatePropertyCommand(this, row, attrName, oldValue, newValue));
    _isDateCacheDirty = true;
}

void MemberInfoController::updateMemberProperty(QUuid id, const QString& attrName, const QVariant& newValue) {
    int row = _model->getRowById(id);
    updateMemberProperty(row, attrName, newValue);
}

void MemberInfoController::updateMemberNote(int row, const QString& newNote) {
    if (_model && row >= 0) {
        _model->updateMemberNote(row, newNote);
    }
}

void MemberInfoController::updatePropertyVisibility(const QStringList& selectedList) {
    if (!_model) return;
    //获取全部属性，并根据选中的列表更新可见性
    QList<QString> allProps = _model->getProperties();
    for (const QString& prop : allProps) {
        _model->setPropertyVisible(prop, selectedList.contains(prop));
    }
}

QModelIndex MemberInfoController::getViewIndex() const{
    return _view->currentIndex();
}

int MemberInfoController::getCurrentRow() const {
    return _view ? _view->currentIndex().row() : -1;
}

void MemberInfoController::removeProperty(const QString& attrName) {
    bool wasVisible = _model->getShowProperties().value(attrName, false).toBool();
    QMap<QUuid, QString> cachedData;
    for (int i = 0; i < _model->rowCount(); ++i) {
        MemberInfoModel::MemberData* ptr = _model->getMemberDataPtr(i);
        if (ptr && ptr->properties.contains(attrName)) {
            cachedData.insert(ptr->id, ptr->properties.value(attrName).toString());
        }
    }
    _undoStack->push(new MemberRemovePropertyCommand(this, attrName, wasVisible, cachedData));
}

void* MemberInfoController::getMemberDataPtr(int row) {
    if (!_model) return nullptr;
    return static_cast<void*>(_model->getMemberDataPtr(row));
}

int MemberInfoController::addNewProperty(const QString& name) {
    if (_model->getProperties().contains(name)) return 1;
    if (name.isEmpty() || name.simplified().isEmpty()) return 2;
    if (name.size() > 8) return 3;

    _undoStack->push(new MemberAddPropertyCommand(this, name));
    return 0;
}

QUuid MemberInfoController::getIdByName(const QString& name) const {
    return _model->getIdByName(name);
}

QList<QString> MemberInfoController::getAllPropertyNames() const {
    return _model ? _model->_allProperties : QList<QString>();
}

QVariantMap MemberInfoController::getPropertyVisibilityMap() const {
    return _model ? _model->_properties : QVariantMap();
}

QVariantMap MemberInfoController::getMemberDetails(int row) const {
    if (!_model || row < 0 || row >= _model->_members.count()) {
        return QVariantMap();
    }
    return _model->getMemberAllData(row);
}

QString MemberInfoController::getMemberNote(int row) const {
    if (!_model || row < 0 || row >= _model->_members.count()) {
        return QString();
    }
    return _model->_members[row].note;
}

QList<MemberBitSchedule> MemberInfoController::getMemberScheduleSnapshot() const {
    QDate dynamicBase = calculateEarliestMonday();
    IdleMatchService::instance()->setBaseDate(dynamicBase);

    QList<MemberBitSchedule> snapshot;
    if (!_model) return snapshot;

    const auto& members = _model->_members;
    

    for (const auto& member : members) {
        if (!member.enableScheduling) continue;
        auto bitSchPtr = std::make_unique<MemberBitSchedule>();
        bitSchPtr->id = member.id;
        bitSchPtr->name = member.properties.value("姓名").toString();

        for (auto dateIt = member.scheduleMap.constBegin(); dateIt != member.scheduleMap.constEnd(); ++dateIt) {
            QString dateStr = dateIt.key();
            QDate date = QDate::fromString(dateStr, "yyyy-MM-dd");

            if (!date.isValid()) {
                date = QDate::fromString(QString::number(QDate::currentDate().year()) + "-" + dateStr, "yyyy-MM-dd");
            }
            if (!date.isValid()) continue;

            if (!bitSchPtr->dailyMask.contains(date)) {
                bitSchPtr->dailyMask[date].set();
            }

            int weekIndex = dynamicBase.daysTo(date) / 7;
            int dayIdx = date.dayOfWeek() - 1;

            for (auto timeIt = dateIt.value().constBegin(); timeIt != dateIt.value().constEnd(); ++timeIt) {
                if (timeIt.value() == true) {
                    TimeMask busyMask = IdleMatchService::timeStringToMask(timeIt.key());
                    bitSchPtr->dailyMask[date] &= (~busyMask);

                    if (weekIndex >= 0 && weekIndex < 64 && dayIdx >= 0 && dayIdx < 7) {
                        bitSchPtr->weeklyMask[dayIdx][weekIndex] &= (~busyMask);
                    }
                }
            }
        }
        snapshot.append(*bitSchPtr);
    }
    return snapshot;
}

QList<QUuid> MemberInfoController::getAllValidIds() const {
    QList<QUuid> ids;;
    if (_model) {
        for (const auto& m : _model->_members) {
            ids << m.id;
        }
    }
    return ids;
}

QString MemberInfoController::getNameById(const QUuid& id) const {
    return _model->getNameById(id);
}

QDate MemberInfoController::calculateEarliestMonday() const {
    if (!_isDateCacheDirty && _earliestDateCache.isValid()) {
        return _earliestDateCache;
    }

    QDate minDate;
    for (const auto& member : _model->_members) {
        for (auto it = member.scheduleMap.constBegin(); it != member.scheduleMap.constEnd(); ++it) {
            QDate d = QDate::fromString(it.key(), "yyyy-MM-dd");
            if (!d.isValid()) continue;
            if (minDate.isNull() || d < minDate) minDate = d;
        }
    }

    if (minDate.isNull()) minDate = QDate::currentDate();

    int diff = minDate.dayOfWeek() - 1;
    _earliestDateCache = minDate.addDays(-diff);
    _isDateCacheDirty = false;

    return _earliestDateCache;
}

void MemberInfoController::updateDutyCount(int row, int count) {
    if (_model) _model->updateDutyCount(row, count);
}

void MemberInfoController::refreshMemberDutyCount(int row) {
    if (!_model || row < 0 || row >= _model->rowCount()) return;

    QUuid id = _model->_members[row].id;

    auto* eComp = ComponentManager::instance()->getComponent<EditorDutyComponent>();
    if (eComp && eComp->getController()) {
        int count = eComp->getController()->getMemberDutyCount(id);

        _model->updateDutyCount(row, count);
    }
}

int MemberInfoController::getDutyCount(int row) const {
    return _model ? _model->getDutyCount(row) : 0;
}

void MemberInfoController::changeViewClickMode(RTypies::View mode) {
	_view->changeClickMode(mode);
}

QUndoStack* MemberInfoController::getUndoStack() const { 
    return _undoStack; 
}

void MemberInfoController::selectAndShowMember(const QUuid& id) {
    if (!_model || !_view) return;
    int row = _model->getRowById(id);
    if (row >= 0) {
        QModelIndex targetIndex = _model->index(row, 0);
        _view->selectionModel()->setCurrentIndex(targetIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QVariantMap details = _model->getMemberAllData(row);
        Q_EMIT memberSelected(details, row);
    }
}

void MemberInfoController::scrollToRow(int row) {
    if (_view) _view->scrollToRow(row);
}

void MemberInfoController::getSearchData(QStringList& names, QList<QVariantMap>& data) {
    if (!_model) return;
    int count = _model->rowCount();
    names.reserve(count);
    data.reserve(count);

    for (int i = 0; i < count; ++i) {
        QString name = _model->_members[i].properties.value("姓名").toString();
        names << name;

        QVariantMap itemData;
        itemData.insert("row", i);
        itemData.insert("name", name);
        data << itemData;
    }
}

void MemberInfoController::requestSearchSuggestionsUpdate() {
    QMetaObject::invokeMethod(BackendWorker::instance(), [=]() {
        BackendWorker::instance()->searchMember(_model->_members);
    }, Qt::QueuedConnection);
}

void MemberInfoController::updateMemberPropertyDirectly(int row, const QString& attrName, const QVariant& value) { 
    _model->updateMemberProperty(row, attrName, value); 
}
void MemberInfoController::insertMembersDirectly(int startIndex, const QList<QVariantMap>& members) {
    QList<QPair<int, MemberInfoModel::MemberData>> dataToInsert;
    for (const auto& map : members) {
        dataToInsert.append({ map["row"].toInt(), map["data"].value<MemberInfoModel::MemberData>() });
    }
    _model->insertMembersAt(dataToInsert);
}
void MemberInfoController::removeMembersDirectly(int startIndex, int count) { 
    _model->removeRows(startIndex, count, QModelIndex()); 
}
void MemberInfoController::deleteMembersDirectly(const QList<QVariantMap>& removedData) {
    QList<QVariantMap> descData = removedData;
    std::sort(descData.begin(), descData.end(), [](const auto& a, const auto& b) { return a["row"].toInt() > b["row"].toInt(); });
    for (const auto& map : descData) { _model->removeRows(map["row"].toInt(), 1, QModelIndex()); }
}
void MemberInfoController::restoreMembersDirectly(const QList<QVariantMap>& removedData) {
    QList<QVariantMap> ascData = removedData;
    std::sort(ascData.begin(), ascData.end(), [](const auto& a, const auto& b) { return a["row"].toInt() < b["row"].toInt(); });
    QList<QPair<int, MemberInfoModel::MemberData>> dataToInsert;
    for (const auto& map : ascData) { dataToInsert.append({ map["row"].toInt(), map["data"].value<MemberInfoModel::MemberData>() }); }
    _model->insertMembersAt(dataToInsert);
}
void MemberInfoController::addPropertyDirectly(const QString& propName) { 
    _model->addNewProperty(propName); 
}
void MemberInfoController::removePropertyDirectly(const QString& propName) { 
    _model->removeProperty(propName); 
}
void MemberInfoController::restorePropertyDirectly(const QString& propName, bool wasVisible, const QMap<QUuid, QString>& cachedData) { 
    _model->restoreProperty(propName, wasVisible, cachedData); 
}

void MemberInfoController::setEnableScheduling(int row, bool enable) {
    _model->setEnableScheduling(row, enable);
}

void MemberInfoController::setScheduleFileSource(int row, const QString& source) {
    _model->setScheduleFileSource(row, source);
}