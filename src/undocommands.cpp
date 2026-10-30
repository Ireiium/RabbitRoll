#include "undocommands.h"
#include "editordutycontroller.h"
#include "memberinfocontroller.h"
#include "schedulecontroller.h"

EditorDutySetCellCommand::EditorDutySetCellCommand(EditorDutyController* controller, int row, int col,
    const QList<QUuid>& oldIds, const QList<QUuid>& newIds, QUndoCommand* parent)
    : QUndoCommand(parent), _controller(controller), _row(row), _col(col), _oldIds(oldIds), _newIds(newIds) {
    setText(QString("修改排班单元格 (%1, %2)").arg(row).arg(col));
}
void EditorDutySetCellCommand::undo() { 
    _controller->setSelectedIdsDirectly(_row, _col, _oldIds); 
}
void EditorDutySetCellCommand::redo() { 
    _controller->setSelectedIdsDirectly(_row, _col, _newIds);
}

EditorDutyAddTimeCommand::EditorDutyAddTimeCommand(EditorDutyController* controller, const QString& time, QUndoCommand* parent)
    : QUndoCommand(parent), _controller(controller), _time(time) {
    setText(QString("添加时间段: %1").arg(time));
}
void EditorDutyAddTimeCommand::undo() { 
    _controller->removeTimeRowDirectly(_time); 
}
void EditorDutyAddTimeCommand::redo() { 
    _controller->addTimeRowDirectly(_time); 
}

EditorDutyAddColumnCommand::EditorDutyAddColumnCommand(EditorDutyController* controller, int mode, const QString& colTitle, QUndoCommand* parent)
    : QUndoCommand(parent), _controller(controller), _mode(mode), _colTitle(colTitle) {
    setText(QString("添加列: %1").arg(colTitle));
}
void EditorDutyAddColumnCommand::undo() { 
    _controller->removeColumnDirectly(_mode, _colTitle); 
}
void EditorDutyAddColumnCommand::redo() { 
    _controller->addColumnDirectly(_mode, _colTitle);
}

EditorDutyRemoveRowCommand::EditorDutyRemoveRowCommand(EditorDutyController* controller, const QString& timeStr,
    const QHash<QString, QList<QUuid>>& savedIds, const QHash<QString, QStringList>& savedCells, QUndoCommand* parent)
    : QUndoCommand(parent), _controller(controller), _timeStr(timeStr), _savedSelectedIds(savedIds), _savedCellData(savedCells) {
    setText(QString("删除排班时间段: %1").arg(_timeStr));
}
void EditorDutyRemoveRowCommand::undo() { 
    _controller->restoreTimeRowDirectly(_timeStr, _savedCellData, _savedSelectedIds);
}
void EditorDutyRemoveRowCommand::redo() { 
    _controller->removeTimeRowDirectly(_timeStr); 
}

EditorDutyRemoveColumnCommand::EditorDutyRemoveColumnCommand(EditorDutyController* controller, int mode, const QString& colStr,
    const QHash<QString, QList<QUuid>>& savedIds, const QHash<QString, QStringList>& savedCells, QUndoCommand* parent)
    : QUndoCommand(parent), _controller(controller), _colStr(colStr), _mode(mode), _savedSelectedIds(savedIds), _savedCellData(savedCells) {
    setText(QString("删除排班列: %1").arg(_colStr));
}
void EditorDutyRemoveColumnCommand::undo() { 
    _controller->restoreColumnDirectly(_mode, _colStr, _savedCellData, _savedSelectedIds);
}
void EditorDutyRemoveColumnCommand::redo() { 
    _controller->removeColumnDirectly(_mode, _colStr); 
}

EditorDutyEditTimeCommand::EditorDutyEditTimeCommand(EditorDutyController* controller, const QString& oldTime, const QString& newTime, QUndoCommand* parent)
    : QUndoCommand(parent), _controller(controller), _oldTime(oldTime), _newTime(newTime) {
    setText(QString("修改排班时间: %1 -> %2").arg(oldTime).arg(newTime));
}
void EditorDutyEditTimeCommand::undo() {
    _controller->updateTimeRowDirectly(_newTime, _oldTime);
}
void EditorDutyEditTimeCommand::redo() { 
    _controller->updateTimeRowDirectly(_oldTime, _newTime);
}

EditorDutyEditDateCommand::EditorDutyEditDateCommand(EditorDutyController* controller, const QDate& oldDate, const QDate& newDate, QUndoCommand* parent)
    : QUndoCommand(parent), _controller(controller), _oldDate(oldDate), _newDate(newDate) {
    setText(QString("修改排班日期: %1 -> %2").arg(oldDate.toString("MM-dd")).arg(newDate.toString("MM-dd")));
}
void EditorDutyEditDateCommand::undo() { 
    _controller->updateDateSlotDirectly(_newDate, _oldDate); 
}
void EditorDutyEditDateCommand::redo() {
    _controller->updateDateSlotDirectly(_oldDate, _newDate);
}

MemberUpdatePropertyCommand::MemberUpdatePropertyCommand(MemberInfoController* controller, int row, const QString& attrName,
    const QVariant& oldVal, const QVariant& newVal, QUndoCommand* parent)
    : QUndoCommand(parent), _controller(controller), _row(row), _attrName(attrName), _oldVal(oldVal), _newVal(newVal) {
    setText(QString("修改成员属性 '%1'").arg(attrName));
}
void MemberUpdatePropertyCommand::undo() { 
    _controller->updateMemberPropertyDirectly(_row, _attrName, _oldVal); 
}
void MemberUpdatePropertyCommand::redo() { 
    _controller->updateMemberPropertyDirectly(_row, _attrName, _newVal); 
}

MemberAddCommand::MemberAddCommand(MemberInfoController* controller, int startIndex, const QList<QVariantMap>& newMembers, QUndoCommand* parent)
    : QUndoCommand(parent), _controller(controller), _newMembers(newMembers), _startIndex(startIndex) {
    setText(newMembers.size() > 1 ? QString("批量添加 %1 个成员").arg(newMembers.size()) : "添加成员");
}
void MemberAddCommand::undo() { 
    _controller->removeMembersDirectly(_startIndex, _newMembers.size());
}
void MemberAddCommand::redo() { 
    _controller->insertMembersDirectly(_startIndex, _newMembers); 
}

MemberRemoveCommand::MemberRemoveCommand(MemberInfoController* controller, const QList<QVariantMap>& removedData, QUndoCommand* parent)
    : QUndoCommand(parent), _controller(controller), _removedData(removedData) {
    setText(removedData.size() > 1 ? QString("批量删除 %1 个成员").arg(removedData.size()) : "删除成员");
}
void MemberRemoveCommand::undo() { 
    _controller->restoreMembersDirectly(_removedData);
}
void MemberRemoveCommand::redo() { 
    _controller->deleteMembersDirectly(_removedData); 
}

MemberAddPropertyCommand::MemberAddPropertyCommand(MemberInfoController* controller, const QString& propName, QUndoCommand* parent)
    : QUndoCommand(parent), _controller(controller), _propName(propName) {
    setText(QString("添加属性 '%1'").arg(propName));
}
void MemberAddPropertyCommand::undo() { _controller->removePropertyDirectly(_propName); }
void MemberAddPropertyCommand::redo() { _controller->addPropertyDirectly(_propName); }

MemberRemovePropertyCommand::MemberRemovePropertyCommand(MemberInfoController* controller, const QString& propName,
    bool wasVisible, const QMap<QUuid, QString>& cachedData, QUndoCommand* parent)
    : QUndoCommand(parent), _controller(controller), _propName(propName), _wasVisible(wasVisible), _cachedData(cachedData) {
    setText(QString("删除属性 '%1'").arg(propName));
}
void MemberRemovePropertyCommand::undo() { 
    _controller->restorePropertyDirectly(_propName, _wasVisible, _cachedData);
}
void MemberRemovePropertyCommand::redo() { 
    _controller->removePropertyDirectly(_propName); 
}

ScheduleToggleCommand::ScheduleToggleCommand(QUuid memberId, const QString& date, const QString& time, MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent)
    : QUndoCommand(parent), _memberId(memberId), _dateKey(date), _timeKey(time), _mController(mController), _sController(sController) {
    setText(QString("更改排班状态: %1 %2").arg(date).arg(time));
}
void ScheduleToggleCommand::undo() { 
    _mController->selectAndShowMember(_memberId); _sController->toggleStateDirectly(_dateKey, _timeKey);
}
void ScheduleToggleCommand::redo() { 
    _mController->selectAndShowMember(_memberId); _sController->toggleStateDirectly(_dateKey, _timeKey); 
}

ScheduleAddTimeCommand::ScheduleAddTimeCommand(QUuid memberId, const QString& time, MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent)
    : QUndoCommand(parent), _memberId(memberId), _timeKey(time), _mController(mController), _sController(sController) {
    setText(QString("添加排班时间段: %1").arg(time));
}
void ScheduleAddTimeCommand::undo() {
    _mController->selectAndShowMember(_memberId); _sController->removeTimeSlotDirectly(_timeKey); 
}
void ScheduleAddTimeCommand::redo() { 
    _mController->selectAndShowMember(_memberId); _sController->addTimeSlotDirectly(_timeKey);
}

ScheduleEditTimeCommand::ScheduleEditTimeCommand(QUuid memberId, int row, const QString& oldTime, const QString& newTime, MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent)
    : QUndoCommand(parent), _memberId(memberId), _row(row), _oldTime(oldTime), _newTime(newTime), _mController(mController), _sController(sController) {
    setText(QString("编辑排班时间: %1 -> %2").arg(oldTime).arg(newTime));
}
void ScheduleEditTimeCommand::undo() { 
    _mController->selectAndShowMember(_memberId); _sController->editTimeSlotByStringDirectly(_newTime, _oldTime);
}
void ScheduleEditTimeCommand::redo() { 
    _mController->selectAndShowMember(_memberId); _sController->editTimeSlotByStringDirectly(_oldTime, _newTime);
}

ScheduleAddDateCommand::ScheduleAddDateCommand(QUuid memberId, const QString& date, MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent)
    : QUndoCommand(parent), _memberId(memberId), _dateKey(date), _mController(mController), _sController(sController) {
    setText(QString("添加排班日期: %1").arg(date));
}
void ScheduleAddDateCommand::undo() { 
    _mController->selectAndShowMember(_memberId); _sController->removeDateSlotDirectly(_dateKey); 
}
void ScheduleAddDateCommand::redo() { 
    _mController->selectAndShowMember(_memberId); _sController->addDateSlotDirectly(_dateKey);
}

ScheduleRemoveTimeCommand::ScheduleRemoveTimeCommand(QUuid memberId, const QString& time, const QMap<QString, bool>& savedData, MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent)
    : QUndoCommand(parent), _memberId(memberId), _timeStr(time), _savedData(savedData), _mController(mController), _sController(sController) {
    setText(QString("删除个人时间段: %1").arg(time));
}
void ScheduleRemoveTimeCommand::undo() {
    _mController->selectAndShowMember(_memberId); _sController->restoreTimeSlotDirectly(_timeStr, _savedData); 
}
void ScheduleRemoveTimeCommand::redo() {
    _mController->selectAndShowMember(_memberId); _sController->removeTimeSlotDirectly(_timeStr);
}

ScheduleRemoveDateCommand::ScheduleRemoveDateCommand(QUuid memberId, const QString& date, const QMap<QString, bool>& savedData, MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent)
    : QUndoCommand(parent), _memberId(memberId), _dateStr(date), _savedData(savedData), _mController(mController), _sController(sController) {
    setText(QString("删除个人日期列: %1").arg(date));
}
void ScheduleRemoveDateCommand::undo() {
    _mController->selectAndShowMember(_memberId); _sController->restoreDateSlotDirectly(_dateStr, _savedData);
}
void ScheduleRemoveDateCommand::redo() {
    _mController->selectAndShowMember(_memberId); _sController->removeDateSlotDirectly(_dateStr);
}

ScheduleEditDateCommand::ScheduleEditDateCommand(QUuid memberId, const QString& oldDate, const QString& newDate, MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent)
    : QUndoCommand(parent), _memberId(memberId), _oldDate(oldDate), _newDate(newDate), _mController(mController), _sController(sController) {
    setText(QString("修改个人排班日期: %1 -> %2").arg(oldDate).arg(newDate));
}
void ScheduleEditDateCommand::undo() { 
    _mController->selectAndShowMember(_memberId); _sController->updateDateSlotDirectly(_newDate, _oldDate);
}
void ScheduleEditDateCommand::redo() {
    _mController->selectAndShowMember(_memberId); _sController->updateDateSlotDirectly(_oldDate, _newDate);
}

ScheduleClearCellsCommand::ScheduleClearCellsCommand(QUuid memberId, const QList<QPair<QString, QString>>& targetCells,
    MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent)
    : QUndoCommand(parent), _memberId(memberId), _cells(targetCells), _mController(mController), _sController(sController)
{
    setText("批量清空单元格");
}

void ScheduleClearCellsCommand::undo() {
    _mController->selectAndShowMember(_memberId);
    for (const auto& cell : _cells) {
        _sController->setCellStateDirectly(cell.first, cell.second, true);
    }
}

void ScheduleClearCellsCommand::redo() {
    _mController->selectAndShowMember(_memberId);
    for (const auto& cell : _cells) {
        _sController->setCellStateDirectly(cell.first, cell.second, false);
    }
}
