#pragma once

#include <QObject>
#include <QUuid>
#include "RTypies.h"
#include <QModelIndex>
#include <QVariant>
#include <QHash>
#include <QUndoStack>

class EditorDutyView;
class EditorDutyModel;
class QLayout;


class EditorDutyController : public QObject
{
    Q_OBJECT

friend class IdleMatchService;

Q_SIGNALS:
    void cellDoubleClicked(int row, int col);
    void addTimeError(RTypies::Message key);
    void popWinDialogRequest(const RTypies::WinDialog key, QVariant data);
    void scheduleFinish(const RTypies::Message msg, const QString& debugMsg);
    void cellDataChanged(int row, int col);

public Q_SLOTS:
    void deleteTimeRow(int index);
    void deleteTimeRows(const QList<int>& rows);
    void deleteColumn(int index);
    void deleteColumns(const QList<int>& columns);
    void handleEditRow(int index);
    void applyAutoScheduleResults(const QHash<QString, QList<QUuid>>& schedule);
    void applyPythonScheduleResults(const QHash<QString, QList<QUuid>>& schedule, const QString& debugMsg);
    void applyAIResult(bool success, const QHash<QString, QList<QUuid>>& schedule, const QString& debugMsg);
    void updateColumnDate(int index, const QDate& newDate);

public:
    EditorDutyController(EditorDutyModel* model, EditorDutyView* view, QObject* parent = nullptr);

    void editTimeRow(int index, const int hour1, const int minute1, const int hour2, const int minute2);
    void setupView(QLayout* layout);
    void addTimeRow(const int hour1, const int minute1, const int hour2, const int minute2);
    void addColumn();
    void switchMode();
    void updateViewHeight();
    QModelIndex getIndex(int row, int col); 
    QStringList getTimesRow() const;
    QStringList getColumns() const;
    int getMode() const;

    QList<QUuid> getSelectedIds(const QModelIndex& index) const;
    void applyCalculationResults(const QHash<QString, QStringList>& schedule);
    void setSelectedIds(const QModelIndex& index, const QList<QUuid>& ids);

    void autoSchedule(int targetPerCell, bool enablePrioritize, bool isMultipate);
    void PythonSchedule(const QString& scriptPath, int targetPerCell);

    int getMemberDutyCount(const QUuid& id) const;

    void handleClearCellsRequest(const QModelIndexList& indices);

    void abortSchedule();

    QUndoStack* getUndoStack() const;
    void setSelectedIdsDirectly(int row, int col, const QList<QUuid>& ids);
    void addTimeRowDirectly(const QString& time);
    void removeTimeRowDirectly(const QString& time);
    void restoreTimeRowDirectly(const QString& time, const QHash<QString, QStringList>& cellData, const QHash<QString, QList<QUuid>>& selIds);
    void addColumnDirectly(int mode, const QString& colTitle);
    void removeColumnDirectly(int mode, const QString& colTitle);
    void restoreColumnDirectly(int mode, const QString& colTitle, const QHash<QString, QStringList>& cellData, const QHash<QString, QList<QUuid>>& selIds);
    void updateTimeRowDirectly(const QString& oldTime, const QString& newTime);
    void updateDateSlotDirectly(const QDate& oldDate, const QDate& newDate);


#ifdef ENABLE_AI_SUPPORT
    void requestAISchedule(const QString& userPrompt, QDate startDate, QDate endDate, bool enableThinking, const QSet<QString>& properties = {});
    /*QString extractDateFromKey(const QString& key);*/
#endif

private:
    EditorDutyModel* _model{ nullptr };
    EditorDutyView* _view{ nullptr };
    QUndoStack* _undoStack{ nullptr };
};
