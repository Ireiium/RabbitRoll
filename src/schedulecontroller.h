#pragma once
#include <QObject>
#include <QLayout>
#include "RTypies.h"
#include <QJsonObject>
#include <QModelIndexList>

class ScheduleModel;
class ScheduleView;
class MemberInfoController;

class ScheduleController : public QObject
{
    Q_OBJECT

Q_SIGNALS:
    void addNewTimeRowFail(RTypies::Message key);
    void popWinDialogRequest(const RTypies::WinDialog key, QVariant data);

public Q_SLOTS:
    void onMemberDataSelected(void* data);
    void handleEditRowRequest(int logicalIndex);
    void handleClearCellsRequest(const QModelIndexList& indices);

    void deleteTimeRow(int logicalIndex);
    void deleteDateColumn(int logicalIndex);
    void updateColumnDate(int logicalIndex, const QDate& newDate);


public:
    ScheduleController(ScheduleModel* model, ScheduleView* view, QObject* parent = nullptr);

    void setupView(QLayout* layout);

    void addNextDay();

    void addTimeSlot(const int hour1, const int minute1, const int hour2, const int minute2);
    void editTimeSlot(int index, const int hour1, const int minute1, const int hour2, const int minute2);

    void updateViewHeight();

    /*QVariant getModelData(RTypies::Model key) const;*/
    int columnCount() const;
    int rowCount() const;

    bool handleImportedSchedule(const QJsonObject& importedData, QWidget* parentWidget, int overwritePolicy = -1);

    void setMemberInfoController(MemberInfoController* mController);
    void requestToggleState(const QModelIndex& index);

    //directly直接给undo/redo执行
    void toggleStateDirectly(const QString& date, const QString& time);
    void addTimeSlotDirectly(const QString& time);
    void removeTimeSlotDirectly(const QString& time);
    void editTimeSlotDirectly(int row, const QString& time);
    void editTimeSlotByStringDirectly(const QString& oldTime, const QString& newTime);
    void addDateSlotDirectly(const QString& date);
    void removeDateSlotDirectly(const QString& date);
    void updateDateSlotByString(const QString& oldDateStr, const QDate& newDate);
    void restoreTimeSlotDirectly(const QString& time, const QMap<QString, bool>& data);
    void restoreDateSlotDirectly(const QString& date, const QMap<QString, bool>& data);
    void updateDateSlotDirectly(const QString& oldDate, const QString& newDate);
    void setCellStateDirectly(const QString& date, const QString& time, bool isBusy);

private:
    ScheduleModel* _model;
    ScheduleView* _view;
    QUuid _currentMemberId;
    MemberInfoController* _mController = nullptr;
};
