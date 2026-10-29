#pragma once

#include <QObject>
#include <QAbstractTableModel>
#include <QVariantMap>
#include "RTypies.h"
#include <QUuid>
#include <QDate>
class  EditorDutyController;
class MemberInfoModel;
class EditorDutyModel  : public QAbstractTableModel
{
	Q_OBJECT


friend class EditorDutyComponent;
friend class EditorDutyController;
friend class IdleMatchService;
friend class ProjectManager;

private Q_SLOTS:
    void onMembersAboutToBeRemoved(const QModelIndex& parent, int first, int last);
    void updateDateSlot(int column, QDate newDate);

public:
    void updateTimeRow(int row, const QString& newTime);
    enum ColumnMode { WeekMode, DateMode };

    EditorDutyModel(QObject* parent = nullptr);
    ~EditorDutyModel();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    bool removeColumns(int column, int count, const QModelIndex& parent = QModelIndex()) override;

    QString getFullDateString(int column) const;

    void setColumnMode(ColumnMode mode);//切换模式
    void addTimeRow(const QString& newTime, const QModelIndex& index, const int first, const int last);//添加时间行
    void addColumn(const QString& colTitle);//添加列（周几或日期）

    void setCellMembers(int row, int col, const QStringList& memberNames);
    QStringList* getTimesRow();

    ColumnMode getColumnMode() const;
    QStringList getWeekColumns() const;
    QStringList getDateColumns() const;

    void refreshView();

    void setSelectedIds(int row, int col, const QList<QUuid>& ids);
    QList<QUuid> getSelectedIds(int row, int col) const;

    const QHash<QString, QList<QUuid>>& getSelectedIdsMap() const;

    void setSelectedIdsMap(const QHash<QString, QList<QUuid>>& map);

    const QHash<QString, QStringList>& getCellDataMap() const;

    //基于字符串定位和恢复数据
    void restoreCellData(const QHash<QString, QStringList>& cellMapData, const QHash<QString, QList<QUuid>>& selMapData);
    void removeTimeRowByString(const QString& timeStr);
    void removeColumnByString(const QString& colStr);
    void updateTimeRowByString(const QString& oldTime, const QString& newTime);
    void updateDateSlotByString(const QString& oldDateStr, const QDate& newDate);

    void reset();
    void clear();

private:
    int getMemberDutyCount(const QUuid& id) const;

    ColumnMode _currentMode{ WeekMode };

    QStringList _timeRows;
    QStringList _weekColumns;
    QStringList _dateColumns;

    QHash<QString, QStringList> _cellDataMap;//发送给dock用，时间段-名字
    QMap<QString, QStringList> _selectedMembersMap;//数据交互用，时间段-UUID

    QString getCellKey(int row, int col) const;

    QHash<QString, QList<QUuid>> _selectedIdsMap;

    MemberInfoModel* _mModel{ nullptr };
};

