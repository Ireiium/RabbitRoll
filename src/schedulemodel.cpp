#include "schedulemodel.h"
#include "memberinfomodel.h"
#include <QDate>
#include <QJsonObject>
#include <algorithm>
#include <QTime>

namespace {
bool validTimeRange(const QString& range) {
    const auto parts = range.split('-');
    if (parts.size() != 2) return false;
    const auto start = QTime::fromString(parts[0], "hh:mm");
    const auto end = QTime::fromString(parts[1], "hh:mm");
    return start.isValid() && end.isValid() && start < end;
}
}

ScheduleModel::ScheduleModel(QObject *parent)
	: QAbstractTableModel(parent)
{}

ScheduleModel::~ScheduleModel()
{}

void ScheduleModel::setScheduleData(QStringList* times, QStringList* dates, QMap<QString, QMap<QString, bool>>* data) {
    beginResetModel();
    _usesMemberBinding = false;
    _memberModel = nullptr;
    _boundMemberId = QUuid();
    _timesPtr = times;
    _datesPtr = dates;
    _mapPtr = data;
    endResetModel();
}

void ScheduleModel::bindMember(MemberInfoModel* model, const QUuid& id) {
    beginResetModel();
    _usesMemberBinding = true;
    _memberModel = model;
    _boundMemberId = id;
    resolveScheduleData();
    endResetModel();
}

void ScheduleModel::resolveScheduleData() const {
    if (!_usesMemberBinding) return;
    auto* member = _memberModel ? _memberModel->getMemberDataPtr(_memberModel->getRowById(_boundMemberId)) : nullptr;
    _timesPtr = member ? &member->scheduleTimes : nullptr;
    _datesPtr = member ? &member->scheduleDates : nullptr;
    _mapPtr = member ? &member->scheduleMap : nullptr;
}

int ScheduleModel::rowCount(const QModelIndex& parent) const {
    resolveScheduleData();
    return !parent.isValid() && _timesPtr ? _timesPtr->count() : 0;
}

int ScheduleModel::columnCount(const QModelIndex& parent) const {
    resolveScheduleData();
    return !parent.isValid() && _datesPtr ? _datesPtr->count() : 0;
}

QVariant ScheduleModel::headerData(int section, Qt::Orientation orientation, int role) const {
    resolveScheduleData();
    if (role != Qt::DisplayRole || section < 0) return QVariant();

    if (orientation == Qt::Horizontal) {
        if (_datesPtr && section < _datesPtr->count()) {
            QString fullDate = _datesPtr->at(section);
            QDate date = QDate::fromString(fullDate, "yyyy-MM-dd");
            if (date.isValid()) {
                return date.toString("MM-dd");
            }
            return fullDate;
        }
    }
    else {
        return (_timesPtr && section < _timesPtr->count()) ? _timesPtr->at(section) : QVariant();
    }
    return QVariant();
}

QVariant ScheduleModel::data(const QModelIndex& index, int role) const {
    resolveScheduleData();
    if (!index.isValid() || index.model() != this || !_mapPtr || !_timesPtr || !_datesPtr
        || index.row() >= rowCount() || index.column() >= columnCount()) return QVariant();

    QString timeKey = _timesPtr->at(index.row());
    QString dateKey = _datesPtr->at(index.column());

    //只有当Delegate请求UserRole时才返回bool
    if (role == Qt::UserRole) {
        return _mapPtr->value(dateKey).value(timeKey, false);
    }
    return QVariant();
}

//点击切换状态
//void ScheduleModel::toggleState(const QModelIndex& index) {
//    if (!index.isValid()) return;
//
//    QString timeKey = _timesPtr->at(index.row());
//    QString dateKey = _datesPtr->at(index.column());
//    //修改主模型内存中的值
//    bool currentState = (*_mapPtr)[dateKey][timeKey];
//    (*_mapPtr)[dateKey][timeKey] = !currentState;
//
//    qDebug() << "Clicking:" << dateKey << timeKey << (*_mapPtr)[dateKey][timeKey];
//
//    Q_EMIT dataChanged(index, index, { Qt::UserRole });
//}

void ScheduleModel::addTimeSlot(const QString& newTime, const QModelIndex& index, const int first, const int last) {
    resolveScheduleData();

    if (!_timesPtr || !_datesPtr || !_mapPtr || !validTimeRange(newTime) || _timesPtr->contains(newTime)) return;
    auto it = std::lower_bound(_timesPtr->begin(), _timesPtr->end(), newTime);
    int insertIdx = std::distance(_timesPtr->begin(), it);

    beginInsertRows(QModelIndex(), insertIdx, insertIdx);
    _timesPtr->insert(insertIdx, newTime);

    for (const QString& date : *_datesPtr) {
        (*_mapPtr)[date][newTime] = false;
    }
    endInsertRows();
}

void ScheduleModel::addDateSlot(const QString& newDate, const QModelIndex& index, const int first, const int last) {
    resolveScheduleData();
    if (!_timesPtr || !_datesPtr || !_mapPtr || !QDate::fromString(newDate, "yyyy-MM-dd").isValid()
        || _datesPtr->contains(newDate)) return;
    auto it = std::lower_bound(_datesPtr->begin(), _datesPtr->end(), newDate);
    int insertIdx = int(std::distance(_datesPtr->begin(), it));
    beginInsertColumns(QModelIndex(), insertIdx, insertIdx);
    _datesPtr->insert(insertIdx, newDate);
    for (const QString& time : *_timesPtr) {
        (*_mapPtr)[newDate][time] = false;
    }
    endInsertColumns();
}

QString ScheduleModel::getLastDate() const {
    resolveScheduleData();
    return _datesPtr && !_datesPtr->isEmpty() ? _datesPtr->last() : QString();
}

QStringList* ScheduleModel::getTimesRow() const {
    resolveScheduleData();
    return _timesPtr;
}

bool ScheduleModel::removeRows(int row, int count, const QModelIndex& parent) {
    resolveScheduleData();
    if (!_timesPtr || !_mapPtr || parent.isValid() || row < 0 || count <= 0
        || row > _timesPtr->count() || count > _timesPtr->count() - row) return false;
    beginRemoveRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        QString timeKey = _timesPtr->at(row);
        for (auto it = _mapPtr->begin(); it != _mapPtr->end(); ++it) {
            it.value().remove(timeKey);
        }
        _timesPtr->removeAt(row);
    }
    endRemoveRows();
    return true;
}

bool ScheduleModel::removeColumns(int column, int count, const QModelIndex& parent) {
    resolveScheduleData();
    if (!_datesPtr || !_mapPtr || parent.isValid() || column < 0 || count <= 0
        || column > _datesPtr->count() || count > _datesPtr->count() - column) return false;
    beginRemoveColumns(parent, column, column + count - 1);
    for (int i = 0; i < count; ++i) {
        QString dateKey = _datesPtr->at(column);
        if (_mapPtr) {
            _mapPtr->remove(dateKey);
        }
        _datesPtr->removeAt(column);
    }
    endRemoveColumns();
    return true;
}

void ScheduleModel::updateTimeSlot(int row, const QString& newTime) {
    resolveScheduleData();
    if (!_timesPtr || !_mapPtr || row < 0 || row >= _timesPtr->count() || !validTimeRange(newTime)) return;

    QString oldTime = _timesPtr->at(row);
    if (oldTime == newTime || _timesPtr->contains(newTime)) return;
    beginResetModel();

    for (auto it = _mapPtr->begin(); it != _mapPtr->end(); ++it) {
        QMap<QString, bool>& timeMap = it.value();
        if (timeMap.contains(oldTime)) {
            bool state = timeMap.take(oldTime);
            timeMap.insert(newTime, state);
        }
        else {
            timeMap.insert(newTime, false);
        }
    }

    _timesPtr->replace(row, newTime);

    std::sort(_timesPtr->begin(), _timesPtr->end());

    endResetModel();
}

void ScheduleModel::updateDateSlot(int column, QDate newDate) {
    resolveScheduleData();
    if (!_datesPtr || !_mapPtr || column < 0 || column >= _datesPtr->count() || !newDate.isValid()) return;

    QString newDateStr = newDate.toString("yyyy-MM-dd");
    QString oldDateStr = _datesPtr->at(column);

    if (newDateStr == oldDateStr) return;
    if (_datesPtr->contains(newDateStr)) {
        Q_EMIT editTimeError();
        return;
    }

    beginResetModel();
    if (_mapPtr->contains(oldDateStr)) {
        QMap<QString, bool> rowData = _mapPtr->take(oldDateStr);
        _mapPtr->insert(newDateStr, rowData);
    }
    _datesPtr->replace(column, newDateStr);
    std::sort(_datesPtr->begin(), _datesPtr->end(), [](const QString& a, const QString& b) {
        return a < b;
        });

    endResetModel();
}

QMap<QString, bool> ScheduleModel::getTimeSlotData(const QString& time) const {
    resolveScheduleData();
    QMap<QString, bool> data;
    if (!_mapPtr) return data;
    for (auto it = _mapPtr->begin(); it != _mapPtr->end(); ++it) {
        if (it.value().contains(time)) {
            data.insert(it.key(), it.value().value(time));
        }
    }
    return data;
}

QMap<QString, bool> ScheduleModel::getDateSlotData(const QString& date) const {
    resolveScheduleData();
    if (_mapPtr && _mapPtr->contains(date)) return _mapPtr->value(date);
    return QMap<QString, bool>();
}

void ScheduleModel::restoreTimeSlot(const QString& time, const QMap<QString, bool>& data) {
    resolveScheduleData();
    if (!_timesPtr || !_datesPtr || !_mapPtr || !validTimeRange(time)) return;

    beginResetModel();
    if (!_timesPtr->contains(time)) {
        _timesPtr->append(time);
        std::sort(_timesPtr->begin(), _timesPtr->end(), [](const QString& a, const QString& b) {
            return a < b;
            });
    }
    for (const QString& dateKey : *_datesPtr) {
        (*_mapPtr)[dateKey][time] = data.value(dateKey, false);
    }

    endResetModel();
}

void ScheduleModel::restoreDateSlot(const QString& date, const QMap<QString, bool>& data) {
    resolveScheduleData();
    if (!_timesPtr || !_datesPtr || !_mapPtr || !QDate::fromString(date, "yyyy-MM-dd").isValid()) return;

    beginResetModel();
    if (!_datesPtr->contains(date)) {
        _datesPtr->append(date);
        std::sort(_datesPtr->begin(), _datesPtr->end(), [](const QString& a, const QString& b) {
            return a < b;
            });
    }

    if (!_mapPtr->contains(date)) {
        (*_mapPtr)[date] = QMap<QString, bool>();
    }
    for (const QString& timeKey : *_timesPtr) {
        (*_mapPtr)[date][timeKey] = data.value(timeKey, false);
    }

    endResetModel();
}

void ScheduleModel::updateDateSlotByString(const QString& oldDateStr, const QDate& newDate) {
    resolveScheduleData();
    if (!_datesPtr) return;
    int column = _datesPtr->indexOf(oldDateStr);

    if (column != -1) {
        this->updateDateSlot(column, newDate);
    }
}

void ScheduleModel::toggleStateByString(const QString& dateKey, const QString& timeKey) {
    resolveScheduleData();
    if (!_mapPtr || !_timesPtr || !_datesPtr || !_mapPtr->contains(dateKey) || !(*_mapPtr)[dateKey].contains(timeKey)) return;
    (*_mapPtr)[dateKey][timeKey] = !(*_mapPtr)[dateKey][timeKey];

    int row = _timesPtr->indexOf(timeKey);
    int col = _datesPtr->indexOf(dateKey);
    if (row >= 0 && col >= 0) {
        QModelIndex idx = index(row, col);
        Q_EMIT dataChanged(idx, idx, { Qt::UserRole });
    }
}

void ScheduleModel::removeTimeSlotByString(const QString& time) {
    resolveScheduleData();
    if (!_timesPtr) return;
    int row = _timesPtr->indexOf(time);
    if (row >= 0) {
        removeRows(row, 1, QModelIndex());
    }
}

void ScheduleModel::removeDateSlotByString(const QString& date) {
    resolveScheduleData();
    if (!_datesPtr) return;
    int col = _datesPtr->indexOf(date);
    if (col >= 0) {
        removeColumns(col, 1, QModelIndex());
    }
}

QString ScheduleModel::getFullDateString(int column) const {
    resolveScheduleData();
    if (_datesPtr && column >= 0 && column < _datesPtr->count()) {
        return _datesPtr->at(column);
    }
    return QString();
}

void ScheduleModel::setCellStateByString(const QString& dateKey, const QString& timeKey, bool state) {
    resolveScheduleData();
    if (_mapPtr && _timesPtr && _datesPtr && _datesPtr->contains(dateKey) && _timesPtr->contains(timeKey)) {
        (*_mapPtr)[dateKey][timeKey] = state;
        int col = _datesPtr->indexOf(dateKey);
        int row = _timesPtr->indexOf(timeKey);
        if (col != -1 && row != -1) {
            QModelIndex idx = index(row, col);
            Q_EMIT dataChanged(idx, idx, { Qt::DisplayRole, Qt::BackgroundRole, Qt::UserRole });
        }
    }
}

bool ScheduleModel::checkConflict(const QJsonObject& importedData) const {
    resolveScheduleData();
    if (!_mapPtr) return false;
    for (auto it = importedData.begin(); it != importedData.end(); ++it) {
        const auto existing = _mapPtr->constFind(it.key());
        if (existing == _mapPtr->constEnd()) continue;
        const auto timeObj = it.value().toObject();
        for (auto timeIt = timeObj.begin(); timeIt != timeObj.end(); ++timeIt) {
            if (timeIt.value().isBool() && existing->contains(timeIt.key())
                && existing->value(timeIt.key()) != timeIt.value().toBool()) return true;
        }
    }
    return false;
}

void ScheduleModel::mergeSchedule(const QJsonObject& importedData, bool overwrite) {
    resolveScheduleData();
    if (!_timesPtr || !_datesPtr || !_mapPtr) return;
    beginResetModel();
    for (auto it = importedData.begin(); it != importedData.end(); ++it) {
        const QString dateKey = it.key();
        if (!QDate::fromString(dateKey, "yyyy-MM-dd").isValid() || !it.value().isObject()) continue;
        const auto timeObj = it.value().toObject();
        for (auto timeIt = timeObj.begin(); timeIt != timeObj.end(); ++timeIt) {
            const QString timeKey = timeIt.key();
            if (!validTimeRange(timeKey) || !timeIt.value().isBool()) continue;
            const bool hasExisting = _mapPtr->contains(dateKey) && _mapPtr->value(dateKey).contains(timeKey);
            if (!_datesPtr->contains(dateKey)) _datesPtr->append(dateKey);
            if (!_timesPtr->contains(timeKey)) _timesPtr->append(timeKey);
            if (!hasExisting || overwrite) (*_mapPtr)[dateKey][timeKey] = timeIt.value().toBool();
        }
    }
    for (const QString& date : *_datesPtr) {
        for (const QString& time : *_timesPtr) {
            if (!(*_mapPtr)[date].contains(time)) (*_mapPtr)[date][time] = false;
        }
    }
    std::sort(_datesPtr->begin(), _datesPtr->end());
    std::sort(_timesPtr->begin(), _timesPtr->end());
    endResetModel();
}

//void ScheduleModel::clearCells(const QModelIndexList& indices) {
//    if (indices.isEmpty()) return;
//
//    for (const QModelIndex& index : indices) {
//        if (!index.isValid()) continue;
//
//        QString dateKey = _datesPtr->at(index.column());
//        QString timeKey = _timesPtr->at(index.row());
//
//        if (_mapPtr->contains(dateKey) && (*_mapPtr)[dateKey].contains(timeKey)) {
//            (*_mapPtr)[dateKey][timeKey] = false;
//            Q_EMIT dataChanged(index, index, { Qt::DisplayRole, Qt::BackgroundRole, Qt::UserRole });
//        }
//    }
//}

QJsonObject ScheduleModel::exportScheduleData() const {
    resolveScheduleData();
    QJsonObject scheduleObj;
    if (!_timesPtr || !_datesPtr || !_mapPtr) return scheduleObj;

    for (const QString& dateKey : *_datesPtr) {
        if (_mapPtr->contains(dateKey)) {
            QJsonObject timeObj;
            for (const QString& timeKey : *_timesPtr) {
                timeObj[timeKey] = (*_mapPtr)[dateKey].value(timeKey, false);
            }
            scheduleObj[dateKey] = timeObj;
        }
    }
    return scheduleObj;
}






