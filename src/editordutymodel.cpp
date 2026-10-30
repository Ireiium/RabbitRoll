#include "editordutymodel.h"
#include "componentmanager.h"
#include "memberinfocomponent.h"

EditorDutyModel::EditorDutyModel(QObject* parent) 
	: QAbstractTableModel(parent) 
{
    auto* eComp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
	_mModel = eComp->getModel();

	connect(_mModel, &MemberInfoModel::dataChanged, this, &EditorDutyModel::refreshView);
	connect(_mModel, &MemberInfoModel::rowsRemoved, this, &EditorDutyModel::refreshView);
}

EditorDutyModel::~EditorDutyModel() {

}

int EditorDutyModel::rowCount(const QModelIndex& parent) const {
	return _timeRows.count();
}

int EditorDutyModel::columnCount(const QModelIndex& parent) const {
	if (_currentMode == WeekMode) {
		return  _weekColumns.count();
	}
	else {
        return _dateColumns.count();
	}
}

QVariant EditorDutyModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (role != Qt::DisplayRole) return QVariant();

	if (orientation == Qt::Vertical) {
		return _timeRows.value(section); //返回时间段
	}
	else {
		return (_currentMode == WeekMode) ? _weekColumns.value(section) : _dateColumns.value(section);
	}
}

QVariant EditorDutyModel::data(const QModelIndex& index, int role) const {
	if (!index.isValid()) return QVariant();

	QString key = getCellKey(index.row(), index.column());

    //显示内容
	if (role == Qt::DisplayRole) {
		QList<QUuid> ids = _selectedIdsMap.value(key);
		if (ids.isEmpty()) return QVariant();

		if (!_mModel) return QVariant();

		QStringList displayLines;
		for (const QUuid& id : ids) {
            //目前只拿名字，以后加一个delegate去扩展
			displayLines << _mModel->getNameById(id);
		}

		return displayLines.join("\n");
	}

    //供dock的数据
	if (role == Qt::UserRole) {
		return _cellDataMap.value(key);
	}

	return QVariant();
}

Qt::ItemFlags EditorDutyModel::flags(const QModelIndex& index) const {
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void EditorDutyModel::setColumnMode(ColumnMode mode) {
	if (_currentMode == mode) return;
	beginResetModel();
	_currentMode = mode;
	endResetModel();
}

void EditorDutyModel::addTimeRow(const QString& newTime, const QModelIndex& index, const int first, const int last) {
	auto it = std::lower_bound(_timeRows.begin(), _timeRows.end(), newTime);
	int insertIdx = std::distance(_timeRows.begin(), it);

	beginInsertRows(QModelIndex(), insertIdx, insertIdx);
	_timeRows.insert(insertIdx, newTime);
	endInsertRows();
}

void EditorDutyModel::addColumn(const QString& colTitle) {
	if (_currentMode == WeekMode && _weekColumns.contains(colTitle)) return;

	beginResetModel();
	if (_currentMode == WeekMode) {
		_weekColumns << colTitle;
		static QMap<QString, int> weekWeight = {
			{"周一", 1}, {"周二", 2}, {"周三", 3}, {"周四", 4},
			{"周五", 5}, {"周六", 6}, {"周日", 7}
		};

		std::sort(_weekColumns.begin(), _weekColumns.end(), [&](const QString& a, const QString& b) {
			return weekWeight.value(a, 99) < weekWeight.value(b, 99);
			});
	}
	else {
		QDate d = QDate::fromString(colTitle, "yyyy-M-d");
		QString standardFormat = d.isValid() ? d.toString("yyyy-MM-dd") : colTitle;

		if (_currentMode == DateMode && _dateColumns.contains(standardFormat)) {
			endResetModel();
			return;
		}

		_dateColumns << standardFormat;

		std::sort(_dateColumns.begin(), _dateColumns.end(), [](const QString& a, const QString& b) {
			QDate dateA = QDate::fromString(a, "yyyy-MM-dd");
			QDate dateB = QDate::fromString(b, "yyyy-MM-dd");
			if (dateA.isValid() && dateB.isValid()) {
				return dateA < dateB;
			}
			return a < b;
			});
	}
	endResetModel();
}

void EditorDutyModel::setCellMembers(int row, int col, const QStringList& memberNames) {
	QString key = getCellKey(row, col);
	_cellDataMap[key] = memberNames;
	Q_EMIT dataChanged(index(row, col), index(row, col), { Qt::DisplayRole });
}

QString EditorDutyModel::getCellKey(int row, int col) const {
	QString rowTitle = _timeRows.value(row);
	QString colTitle = (_currentMode == WeekMode) ? _weekColumns.value(col) : _dateColumns.value(col);
	//拼接成"0_08:00-09:00_周一"或者"1_08:00-09:00_2024-03-20"
	return QString::number(_currentMode) + "_" + rowTitle + "_" + colTitle;
}

QStringList* EditorDutyModel::getTimesRow(){
	QStringList* list = &_timeRows;
	return list;
}

EditorDutyModel::ColumnMode EditorDutyModel::getColumnMode() const { 
	return _currentMode;
}
QStringList EditorDutyModel::getWeekColumns() const { 
	return _weekColumns; 
}
QStringList EditorDutyModel::getDateColumns() const { 
	return _dateColumns; 
}

void EditorDutyModel::refreshView() {
	int rowCount = this->rowCount();
	int colCount = this->columnCount();

	if (rowCount > 0 && colCount > 0) {
		QModelIndex topLeft = index(0, 0);
		QModelIndex bottomRight = index(rowCount - 1, colCount - 1);
		Q_EMIT dataChanged(topLeft, bottomRight);
	}
}

QList<QUuid> EditorDutyModel::getSelectedIds(int row, int col) const {
	return _selectedIdsMap.value(getCellKey(row, col));
}

void EditorDutyModel::setSelectedIds(int row, int col, const QList<QUuid>& ids) {
	if (row < 0 || col < 0) return;
	if (getSelectedIds(row, col) == ids) return;
	_selectedIdsMap[getCellKey(row, col)] = ids;
	Q_EMIT dataChanged(index(row, col), index(row, col), { Qt::DisplayRole });
	Q_EMIT assignmentsChanged();
}

const QHash<QString, QList<QUuid>>& EditorDutyModel::getSelectedIdsMap() const { 
	return _selectedIdsMap; 
}

void EditorDutyModel::setSelectedIdsMap(const QHash<QString, QList<QUuid>>& map) {
	beginResetModel();

    //取当前模式的前缀，0代表周视图，1代表日期视图
	QString modePrefix = QString::number(_currentMode) + "_";

	QList<QString> keysToRemove;
	for (auto it = _selectedIdsMap.begin(); it != _selectedIdsMap.end(); ++it) {
		if (it.key().startsWith(modePrefix)) {
			keysToRemove.append(it.key());
		}
	}
	for (const QString& k : keysToRemove) {
		_selectedIdsMap.remove(k);
	}

	for (auto it = map.begin(); it != map.end(); ++it) {
		_selectedIdsMap.insert(it.key(), it.value());
	}

	endResetModel();
}
void EditorDutyModel::onMembersAboutToBeRemoved(const QModelIndex& parent, int first, int last) {
	if (!_mModel) return;
	QSet<QUuid> idsToRemove;
	for (int i = first; i <= last; ++i) {
		auto* data = _mModel->getMemberDataPtr(i);
		if (data) idsToRemove.insert(data->id);
	}
	beginResetModel();
	for (auto it = _selectedIdsMap.begin(); it != _selectedIdsMap.end(); ++it) {
		QList<QUuid>& idList = it.value();
		for (const QUuid& id : idsToRemove) {
			idList.removeAll(id);
		}
	}
	endResetModel();
}

void EditorDutyModel::reset() {
	beginResetModel();
	clear();
	endResetModel();
}

void EditorDutyModel::clear() {
	_selectedIdsMap.clear();
	_timeRows.clear();//清理时间段定义
	_weekColumns.clear();//清理周视图列
	_dateColumns.clear();//清理日期视图列
	_cellDataMap.clear();
}

void EditorDutyModel::updateTimeRow(int row, const QString& newTime) {
	if (row < 0 || row >= _timeRows.count()) return;

	QString oldTime = _timeRows.at(row);
	if (oldTime == newTime) return;

	beginResetModel();

	QList<QString> keysToUpdateSel;
	for (auto it = _selectedIdsMap.begin(); it != _selectedIdsMap.end(); ++it) {
		QStringList parts = it.key().split('_');
		if (parts.size() >= 3 && parts[1] == oldTime) {
			keysToUpdateSel.append(it.key());
		}
	}
	for (const QString& oldKey : keysToUpdateSel) {
		QStringList parts = oldKey.split('_');
		parts[1] = newTime;
		QString newKey = parts.join('_');
		_selectedIdsMap.insert(newKey, _selectedIdsMap.take(oldKey));
	}

	QList<QString> keysToUpdateCell;
	for (auto it = _cellDataMap.begin(); it != _cellDataMap.end(); ++it) {
		QStringList parts = it.key().split('_');
		if (parts.size() >= 3 && parts[1] == oldTime) {
			keysToUpdateCell.append(it.key());
		}
	}
	for (const QString& oldKey : keysToUpdateCell) {
		QStringList parts = oldKey.split('_');
		parts[1] = newTime;
		QString newKey = parts.join('_');
		_cellDataMap.insert(newKey, _cellDataMap.take(oldKey));
	}

	_timeRows.replace(row, newTime);
	std::sort(_timeRows.begin(), _timeRows.end());

	endResetModel();

	Q_EMIT headerDataChanged(Qt::Vertical, 0, _timeRows.count() - 1);
}

bool EditorDutyModel::removeRows(int row, int count, const QModelIndex& parent) {
	if (row < 0 || row + count > _timeRows.count()) return false;

	beginRemoveRows(parent, row, row + count - 1);
	for (int i = 0; i < count; ++i) {
		QString timeKey = _timeRows.at(row);

		QList<QString> keysToRemove;
		for (auto it = _selectedIdsMap.begin(); it != _selectedIdsMap.end(); ++it) {
			QStringList parts = it.key().split('_');
			if (parts.size() >= 3 && parts[1] == timeKey) {
				keysToRemove.append(it.key());
			}
		}
		for (auto it = _cellDataMap.begin(); it != _cellDataMap.end(); ++it) {
			QStringList parts = it.key().split('_');
			if (parts.size() >= 3 && parts[1] == timeKey) {
				if (!keysToRemove.contains(it.key())) {
					keysToRemove.append(it.key());
				}
			}
		}
		for (const QString& k : keysToRemove) {
			_selectedIdsMap.remove(k);
			_cellDataMap.remove(k);
		}
		_timeRows.removeAt(row);
	}
	endRemoveRows();
	return true;
}

bool EditorDutyModel::removeColumns(int column, int count, const QModelIndex& parent) {
	int colCount = (_currentMode == WeekMode) ? _weekColumns.count() : _dateColumns.count();
	if (column < 0 || column + count > colCount) return false;

	beginRemoveColumns(parent, column, column + count - 1);
	for (int i = 0; i < count; ++i) {
		QString colKey = (_currentMode == WeekMode) ? _weekColumns.at(column) : _dateColumns.at(column);
		QList<QString> keysToRemove;
		for (auto it = _selectedIdsMap.begin(); it != _selectedIdsMap.end(); ++it) {
			QStringList parts = it.key().split('_');
			if (parts.size() >= 3 && parts[2] == colKey) {
				keysToRemove.append(it.key());
			}
		}
		for (auto it = _cellDataMap.begin(); it != _cellDataMap.end(); ++it) {
			QStringList parts = it.key().split('_');
			if (parts.size() >= 3 && parts[2] == colKey) {
				if (!keysToRemove.contains(it.key())) {
					keysToRemove.append(it.key());
				}
			}
		}
		for (const QString& k : keysToRemove) {
			_selectedIdsMap.remove(k);
			_cellDataMap.remove(k);
		}
		if (_currentMode == WeekMode) {
			_weekColumns.removeAt(column);
		}
		else {
			_dateColumns.removeAt(column);
		}
	}
	endRemoveColumns();
	return true;
}

void EditorDutyModel::updateDateSlot(int column, QDate newDate) {
	if (_currentMode != DateMode) return;
	if (column < 0 || column >= _dateColumns.count()) return;

	QString newDateStr = newDate.toString("yyyy-MM-dd");
	QString oldDateStr = _dateColumns.at(column);

	if (newDateStr == oldDateStr) return;
	if (_dateColumns.contains(newDateStr)) return;

	beginResetModel();

	QList<QString> keysToUpdateSel;
	for (auto it = _selectedIdsMap.begin(); it != _selectedIdsMap.end(); ++it) {
		QStringList parts = it.key().split('_');
		if (parts.size() >= 3 && parts[2] == oldDateStr && parts[0] == QString::number(DateMode)) {
			keysToUpdateSel.append(it.key());
		}
	}
	for (const QString& oldKey : keysToUpdateSel) {
		QStringList parts = oldKey.split('_');
		parts[2] = newDateStr;
		QString newKey = parts.join('_');
		_selectedIdsMap.insert(newKey, _selectedIdsMap.take(oldKey));
	}

	QList<QString> keysToUpdateCell;
	for (auto it = _cellDataMap.begin(); it != _cellDataMap.end(); ++it) {
		QStringList parts = it.key().split('_');
		if (parts.size() >= 3 && parts[2] == oldDateStr && parts[0] == QString::number(DateMode)) {
			keysToUpdateCell.append(it.key());
		}
	}
	for (const QString& oldKey : keysToUpdateCell) {
		QStringList parts = oldKey.split('_');
		parts[2] = newDateStr;
		QString newKey = parts.join('_');
		_cellDataMap.insert(newKey, _cellDataMap.take(oldKey));
	}

	_dateColumns.replace(column, newDateStr);

	std::sort(_dateColumns.begin(), _dateColumns.end(), [](const QString& a, const QString& b) {
		QDate dateA = QDate::fromString(a, "yyyy-M-d");
		QDate dateB = QDate::fromString(b, "yyyy-M-d");
		if (dateA.isValid() && dateB.isValid()) {
			return dateA < dateB;
		}
		return a < b;
		});

	endResetModel();

	Q_EMIT headerDataChanged(Qt::Horizontal, 0, _dateColumns.count() - 1);
}

QString EditorDutyModel::getFullDateString(int column) const {
	if (_currentMode == DateMode && column >= 0 && column < _dateColumns.count()) {
		return _dateColumns.at(column);
	}
	return QString();
}

const QHash<QString, QStringList>& EditorDutyModel::getCellDataMap() const { 
	return _cellDataMap; 
}

int EditorDutyModel::getMemberDutyCount(const QUuid& id) const {
	int count = 0;
	for (auto it = _selectedIdsMap.constBegin(); it != _selectedIdsMap.constEnd(); ++it) {
		if (it.value().contains(id)) {
			count++;
		}
	}
	return count;
}

void EditorDutyModel::restoreCellData(const QHash<QString, QStringList>& cellMapData, const QHash<QString, QList<QUuid>>& selMapData) {
	beginResetModel();
	for (auto it = cellMapData.begin(); it != cellMapData.end(); ++it) {
		_cellDataMap.insert(it.key(), it.value());
	}
	for (auto it = selMapData.begin(); it != selMapData.end(); ++it) {
		_selectedIdsMap.insert(it.key(), it.value());
	}
	endResetModel();
}

void EditorDutyModel::removeTimeRowByString(const QString& timeStr) {
	int row = _timeRows.indexOf(timeStr);
	if (row != -1) removeRows(row, 1, QModelIndex());
}

void EditorDutyModel::removeColumnByString(const QString& colStr) {
	int col = (_currentMode == WeekMode) ? _weekColumns.indexOf(colStr) : _dateColumns.indexOf(colStr);
	if (col != -1) removeColumns(col, 1, QModelIndex());
}

void EditorDutyModel::updateTimeRowByString(const QString& oldTime, const QString& newTime) {
	int row = _timeRows.indexOf(oldTime);
	if (row != -1) updateTimeRow(row, newTime);
}

void EditorDutyModel::updateDateSlotByString(const QString& oldDateStr, const QDate& newDate) {
	int col = _dateColumns.indexOf(oldDateStr);
	if (col != -1) updateDateSlot(col, newDate);
}

