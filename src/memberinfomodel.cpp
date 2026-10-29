#include "memberinfomodel.h"
#include <QDate>

MemberInfoModel::MemberInfoModel(QObject *parent)
	: QAbstractTableModel(parent)
{
    reset();
}

MemberInfoModel::~MemberInfoModel()
{}

int MemberInfoModel::rowCount(const QModelIndex & parent) const {
    return _members.count();
}

int MemberInfoModel::columnCount(const QModelIndex& parent) const {
    return _visibleColumns.count();
}

QVariant MemberInfoModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            if (section < _visibleColumns.count()) {
                return _visibleColumns[section];
            }
        }
        else if (orientation == Qt::Vertical) {
            return section + 1;
        }
    }
    return QVariant();
}

QVariant MemberInfoModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();

    if (role == Qt::DisplayRole) {
        QString propName = _visibleColumns[index.column()];
        return _members[index.row()].properties.value(propName, "");
    }
    return QVariant();
}

bool MemberInfoModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (index.isValid() && role == Qt::EditRole) {
        int row = index.row();
        int col = index.column();
        QString propName = _visibleColumns[col];
        _members[row].properties[propName] = value.toString();
        Q_EMIT dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });
        return true;
    }
    return false;
}

bool MemberInfoModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (row<0 || row + count>_members.count()) return false;

    beginRemoveRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i) {
        _members.removeAt(row);
    }
    endRemoveRows();
    return true;
}

//添加成员
//void MemberInfoModel::addMember(const QString& name) {
//    beginInsertRows(QModelIndex(), _members.count(), _members.count());
//    MemberData data;
//    data.id = QUuid::createUuid();
//    for (auto it : _allProperties) {
//        if (it == "姓名") {
//            data.properties["姓名"] = name;
//        }
//        else {
//			data.properties[it] = ""; 
//        }
//    }
//
//    data.dutyCount = 0;
//    data.scheduleTimes << "08:00-09:00";
//    QDate today = QDate::currentDate();
//    for (int i = 0; i < 3; ++i) {
//        QString dateStr = today.addDays(i).toString("yyyy-MM-dd");
//        data.scheduleDates << dateStr;
//
//        //为每一天初始化默认的时间段状态为false
//        data.scheduleMap[dateStr]["08:00-09:00"] = false;
//    }
//
//    _members.append(data);
//    endInsertRows();
//}
//
////批量加入
//void MemberInfoModel::addMembers(const QStringList& names) {
//    if (names.isEmpty()) return;
//
//    beginInsertRows(QModelIndex(), _members.count(), _members.count() + names.count() - 1);
//
//    QDate today = QDate::currentDate();
//
//    for (const QString& name : names) {
//        MemberData data;
//        data.id = QUuid::createUuid();
//        for (auto it : _allProperties) {
//            if (it == "姓名") {
//                data.properties["姓名"] = name;
//            }
//            else {
//                data.properties[it] = "";
//            }
//        }
//
//        data.dutyCount = 0;
//        data.scheduleTimes << "08:00-09:00";
//        for (int i = 0; i < 3; ++i) {
//            QString dateStr = today.addDays(i).toString("yyyy-MM-dd");
//            data.scheduleDates << dateStr;
//            data.scheduleMap[dateStr]["08:00-09:00"] = false;
//        }
//        _members.append(data);
//    }
//
//    endInsertRows();
//}
//
//void MemberInfoModel::addMembersBatch(const QList<QVariantMap>& membersData) {
//    if (membersData.isEmpty()) return;
//
//    beginInsertRows(QModelIndex(), _members.count(), _members.count() + membersData.count() - 1);
//
//    QDate today = QDate::currentDate();
//
//    for (const QVariantMap& map : membersData) {
//        MemberData data;
//        data.id = QUuid::createUuid();
//
//        for (const QString& prop : _allProperties) {
//            data.properties[prop] = "";
//        }
//        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
//            data.properties[it.key()] = it.value();
//        }
//        data.dutyCount = 0;
//        data.scheduleTimes << "08:00-09:00";
//        for (int i = 0; i < 3; ++i) {
//            QString dateStr = today.addDays(i).toString("yyyy-MM-dd");
//            data.scheduleDates << dateStr;
//            data.scheduleMap[dateStr]["08:00-09:00"] = false;
//        }
//        _members.append(data);
//    }
//
//    endInsertRows();
//}


bool MemberInfoModel::updateMemberProperty(int row, const QString& attrName, const QVariant& newValue)
{
    if (row < 0 || row >= _members.count()) return false;

    _members[row].properties[attrName] = newValue.toString();
    int visibleCol = _visibleColumns.indexOf(attrName);
    if (visibleCol != -1) {
        QModelIndex vIndex = this->index(row, visibleCol);
        Q_EMIT dataChanged(vIndex, vIndex, { Qt::DisplayRole });
    }
    return true;
}

//设置属性可见性
void MemberInfoModel::setPropertyVisible(const QString& name, bool visible)
{
    if (name == "姓名" || name == "序号") return;

    if (_properties.contains(name)) {
        _properties[name] = visible;

        // 更新 m_visibleColumns 列表
        if (visible) {
            if (!_visibleColumns.contains(name)) {
                beginInsertColumns(QModelIndex(), _visibleColumns.count(), _visibleColumns.count());
                _visibleColumns << name;
                endInsertColumns();
            }
        }
        else {
            int col = _visibleColumns.indexOf(name);
            if (col != -1) {
                beginRemoveColumns(QModelIndex(), col, col);
                _visibleColumns.removeAt(col);
                endRemoveColumns();
            }
        }
    }
}

QVariantMap MemberInfoModel::getMemberAllData(int row) const {
    if (row < 0 || row >= _members.count()) return QVariantMap();
    QVariantMap map = _members[row].properties;
    map["_enableScheduling"] = _members[row].enableScheduling;
    map["_scheduleFileSource"] = _members[row].scheduleFileSource;
    return map;
}

QVariantMap MemberInfoModel::getShowProperties() const
{
    return _properties;
}


QList<QString> MemberInfoModel::getProperties() const
{
    return _allProperties;
}

int MemberInfoModel::addNewProperty(const QString& name)
{
    try {
        if (_properties.contains(name)) {
            return 1;
        }else if (name.isEmpty() || name.simplified().isEmpty()) {
            return 2;
        }
        else if (name.size() > 8) {
            return 3;
        }
        else
        {
            _allProperties << name;
            /*qDebug() << m_allProperties;*/
            _properties.insert(name, false); // 默认不显示

            beginResetModel();
            for (int i = 0; i < _members.size(); ++i) {
                _members[i].properties.insert(name, "");
            }
            endResetModel();
            //Q_EMIT headerDataChanged(Qt::Horizontal, 0, m_visibleColumns.count() - 1);
            Q_EMIT propertyListChanged();
            return 0;
        }
    }
    catch (const std::exception& e) {
        qDebug() << "Error adding new property:" << e.what();
        return -1;
    }

}

void MemberInfoModel::removeProperty(const QString& name)
{
    if (name == "姓名" || name == "序号") return;

    int visibleIdx = _visibleColumns.indexOf(name);

    if (visibleIdx != -1) {
        beginRemoveColumns(QModelIndex(), visibleIdx, visibleIdx);
        _visibleColumns.removeAt(visibleIdx);
        endRemoveColumns();
    }

    _allProperties.removeOne(name);
    _properties.remove(name);

    for (int i = 0; i < _members.size(); ++i) {
        _members[i].properties.remove(name);
    }

    /*qDebug() << "Model已删除属性:" << name;*/
    Q_EMIT propertyListChanged();
}

QString MemberInfoModel::getNote(int row)const{
    return _members[row].note;
}

bool MemberInfoModel::updateMemberNote(int row, QString note) {
    if (row >= 0) {
        _members[row].note = note;
        return true;
    }
    return false;
}

MemberInfoModel::MemberData* MemberInfoModel::getMemberDataPtr(int row) {
    if (row < 0 || row >= _members.size()) {
        return nullptr;
    }
    return &_members[row];
}

QString MemberInfoModel::getNameById(const QUuid& id) const {
    for (const auto& m : _members) {
        if (m.id == id) {
            return m.properties.value("姓名").toString();
        }
    }
    return "";
}

QUuid MemberInfoModel::getIdByName(const QString& name) const {
    for (const auto& m : _members) {
        if (m.properties.value("姓名").toString() == name) {
            return m.id;
        }
    }
    return QUuid();
}

int MemberInfoModel::getRowById(const QUuid& id) const {
    for(int i=0;i<_members.size(); ++i) {
        if (_members[i].id == id) {
            return i;
        }
	}
    return -1;
}

void MemberInfoModel::updateDutyCount(int row, int count) {
    if (row >= 0 && row < _members.count()) {
        _members[row].dutyCount = count;
    }
}

int MemberInfoModel::getDutyCount(int row) const {
    if (row >= 0 && row < _members.count()) {
        return _members[row].dutyCount;
    }
    return 0;
}

void MemberInfoModel::insertMembersAt(const QList<QPair<int, MemberData>>& membersData) {
    for (const auto& pair : membersData) {
        int row = pair.first;
        beginInsertRows(QModelIndex(), row, row);
        _members.insert(row, pair.second);
        endInsertRows();
    }
}

void MemberInfoModel::restoreProperty(const QString& name, bool visible, const QMap<QUuid, QString>& oldValues) {
    if (_allProperties.contains(name)) return;

    _allProperties << name;
    _properties.insert(name, visible);

    if (visible) {
        beginInsertColumns(QModelIndex(), _visibleColumns.count(), _visibleColumns.count());
        _visibleColumns << name;
        endInsertColumns();
    }

    for (int i = 0; i < _members.size(); ++i) {
        QString oldVal = oldValues.value(_members[i].id, "");
        _members[i].properties.insert(name, oldVal);
    }

    Q_EMIT propertyListChanged();
}

MemberInfoModel::MemberData MemberInfoModel::createDefaultMemberData(const QString& name) {
    MemberData data;
    data.id = QUuid::createUuid();
    for (auto it : _allProperties) {
        if (it == "姓名") {
            data.properties["姓名"] = name;
        }
        else {
            data.properties[it] = "";
        }
    }
    data.dutyCount = 0;
    data.scheduleTimes << "08:00-09:00";
    QDate today = QDate::currentDate();
    for (int i = 0; i < 3; ++i) {
        QString dateStr = today.addDays(i).toString("yyyy-MM-dd");
        data.scheduleDates << dateStr;
        data.scheduleMap[dateStr]["08:00-09:00"] = false;
    }
    return data;
}

QList<MemberInfoModel::MemberData> MemberInfoModel::createBatchMembersData(const QStringList& names) {
    QList<MemberData> list;
    for (const QString& name : names) {
        list.append(createDefaultMemberData(name));
    }
    return list;
}

QList<MemberInfoModel::MemberData> MemberInfoModel::createBatchMembersDataWithFormat(const QList<QVariantMap>& membersData) {
    QList<MemberData> list;
    QDate today = QDate::currentDate();

    for (const QVariantMap& map : membersData) {
        MemberData data;
        data.id = QUuid::createUuid();

        for (const QString& prop : _allProperties) {
            data.properties[prop] = "";
        }
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            data.properties[it.key()] = it.value();
        }
        data.dutyCount = 0;
        data.scheduleTimes << "08:00-09:00";
        for (int i = 0; i < 3; ++i) {
            QString dateStr = today.addDays(i).toString("yyyy-MM-dd");
            data.scheduleDates << dateStr;
            data.scheduleMap[dateStr]["08:00-09:00"] = false;
        }
        list.append(data);
    }
    return list;
}


void MemberInfoModel::reset() {
    beginResetModel();
    clear();
    _visibleColumns << "姓名";
    _properties.insert("姓名", true);//默认显示姓名属性
    _allProperties << "姓名";
	Q_EMIT modelIniRest();
    endResetModel();
}

void MemberInfoModel::clear() {
    _members.clear();
    _visibleColumns.clear();
    _allProperties.clear();
    _properties.clear();
}

bool MemberInfoModel::setEnableScheduling(int row, bool enable) {
    if (row < 0 || row >= _members.count()) return false;
    if (_members[row].enableScheduling != enable) {
        _members[row].enableScheduling = enable;
        QModelIndex idx = index(row, 0);
        Q_EMIT dataChanged(idx, idx);
        return true;
    }
    return false;
}

bool MemberInfoModel::setScheduleFileSource(int row, const QString& source) {
    if (row < 0 || row >= _members.count()) return false;
    if (_members[row].scheduleFileSource != source) {
        _members[row].scheduleFileSource = source;
        QModelIndex idx = index(row, 0);
        Q_EMIT dataChanged(idx, idx);
        return true;
    }
    return false;
}



