#pragma once

#include <QUndoCommand>
#include <QVariant>
#include <QList>
#include <QUuid>
#include <QHash>
#include <QMap>
#include <QDate>

class EditorDutyController;
class MemberInfoController;
class ScheduleController;

class EditorDutySetCellCommand : public QUndoCommand {
public:
    EditorDutySetCellCommand(EditorDutyController* controller, int row, int col,
        const QList<QUuid>& oldIds, const QList<QUuid>& newIds, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    EditorDutyController* _controller;
    int _row, _col;
    QList<QUuid> _oldIds, _newIds;
};

class EditorDutyAddTimeCommand : public QUndoCommand {
public:
    EditorDutyAddTimeCommand(EditorDutyController* controller, const QString& time, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    EditorDutyController* _controller;
    QString _time;
};

class EditorDutyAddColumnCommand : public QUndoCommand {
public:
    EditorDutyAddColumnCommand(EditorDutyController* controller, int mode, const QString& colTitle, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    EditorDutyController* _controller;
    int _mode; // 0: WeekMode, 1: DateMode
    QString _colTitle;
};

class EditorDutyRemoveRowCommand : public QUndoCommand {
public:
    EditorDutyRemoveRowCommand(EditorDutyController* controller, const QString& timeStr,
        const QHash<QString, QList<QUuid>>& savedIds, const QHash<QString, QStringList>& savedCells, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    EditorDutyController* _controller;
    QString _timeStr;
    QHash<QString, QList<QUuid>> _savedSelectedIds;
    QHash<QString, QStringList> _savedCellData;
};

class EditorDutyRemoveColumnCommand : public QUndoCommand {
public:
    EditorDutyRemoveColumnCommand(EditorDutyController* controller, int mode, const QString& colStr,
        const QHash<QString, QList<QUuid>>& savedIds, const QHash<QString, QStringList>& savedCells, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    EditorDutyController* _controller;
    QString _colStr;
    int _mode;
    QHash<QString, QList<QUuid>> _savedSelectedIds;
    QHash<QString, QStringList> _savedCellData;
};

class EditorDutyEditTimeCommand : public QUndoCommand {
public:
    EditorDutyEditTimeCommand(EditorDutyController* controller, const QString& oldTime, const QString& newTime, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    EditorDutyController* _controller;
    QString _oldTime, _newTime;
};

class EditorDutyEditDateCommand : public QUndoCommand {
public:
    EditorDutyEditDateCommand(EditorDutyController* controller, const QDate& oldDate, const QDate& newDate, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    EditorDutyController* _controller;
    QDate _oldDate, _newDate;
};

class MemberUpdatePropertyCommand : public QUndoCommand {
public:
    MemberUpdatePropertyCommand(MemberInfoController* controller, int row, const QString& attrName,
        const QVariant& oldVal, const QVariant& newVal, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    MemberInfoController* _controller;
    int _row;
    QString _attrName;
    QVariant _oldVal, _newVal;
};

class MemberAddCommand : public QUndoCommand {
public:
    MemberAddCommand(MemberInfoController* controller, int startIndex, const QList<QVariantMap>& newMembers, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    MemberInfoController* _controller;
    QList<QVariantMap> _newMembers; // 使用 QVariantMap 或 Controller内部结构来存储
    int _startIndex;
};

class MemberRemoveCommand : public QUndoCommand {
public:
    MemberRemoveCommand(MemberInfoController* controller, const QList<QVariantMap>& removedData, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    MemberInfoController* _controller;
    QList<QVariantMap> _removedData;
};

class MemberAddPropertyCommand : public QUndoCommand {
public:
    MemberAddPropertyCommand(MemberInfoController* controller, const QString& propName, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    MemberInfoController* _controller;
    QString _propName;
};

class MemberRemovePropertyCommand : public QUndoCommand {
public:
    MemberRemovePropertyCommand(MemberInfoController* controller, const QString& propName,
        bool wasVisible, const QMap<QUuid, QString>& cachedData, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    MemberInfoController* _controller;
    QString _propName;
    bool _wasVisible;
    QMap<QUuid, QString> _cachedData;
};

class ScheduleToggleCommand : public QUndoCommand {
public:
    ScheduleToggleCommand(QUuid memberId, const QString& date, const QString& time,
        MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    QUuid _memberId;
    QString _dateKey, _timeKey;
    MemberInfoController* _mController;
    ScheduleController* _sController;
};

class ScheduleAddTimeCommand : public QUndoCommand {
public:
    ScheduleAddTimeCommand(QUuid memberId, const QString& time,
        MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    QUuid _memberId;
    QString _timeKey;
    MemberInfoController* _mController;
    ScheduleController* _sController;
};

class ScheduleEditTimeCommand : public QUndoCommand {
public:
    ScheduleEditTimeCommand(QUuid memberId, int row, const QString& oldTime, const QString& newTime,
        MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    QUuid _memberId;
    int _row;
    QString _oldTime, _newTime;
    MemberInfoController* _mController;
    ScheduleController* _sController;
};

class ScheduleAddDateCommand : public QUndoCommand {
public:
    ScheduleAddDateCommand(QUuid memberId, const QString& date,
        MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    QUuid _memberId;
    QString _dateKey;
    MemberInfoController* _mController;
    ScheduleController* _sController;
};

class ScheduleRemoveTimeCommand : public QUndoCommand {
public:
    ScheduleRemoveTimeCommand(QUuid memberId, const QString& time, const QMap<QString, bool>& savedData,
        MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    QUuid _memberId;
    QString _timeStr;
    QMap<QString, bool> _savedData;
    MemberInfoController* _mController;
    ScheduleController* _sController;
};

class ScheduleRemoveDateCommand : public QUndoCommand {
public:
    ScheduleRemoveDateCommand(QUuid memberId, const QString& date, const QMap<QString, bool>& savedData,
        MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    QUuid _memberId;
    QString _dateStr;
    QMap<QString, bool> _savedData;
    MemberInfoController* _mController;
    ScheduleController* _sController;
};

class ScheduleEditDateCommand : public QUndoCommand {
public:
    ScheduleEditDateCommand(QUuid memberId, const QString& oldDate, const QString& newDate,
        MemberInfoController* mController, ScheduleController* sController, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    QUuid _memberId;
    QString _oldDate, _newDate;
    MemberInfoController* _mController;
    ScheduleController* _sController;
};

class ScheduleClearCellsCommand : public QUndoCommand {
public:
    ScheduleClearCellsCommand(QUuid memberId, const QList<QPair<QString, QString>>& targetCells,
        MemberInfoController* mController, ScheduleController* sController,
        QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    QUuid _memberId;
    QList<QPair<QString, QString>> _cells;
    MemberInfoController* _mController;
    ScheduleController* _sController;
};
