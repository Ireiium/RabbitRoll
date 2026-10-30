#pragma once

#include <QAbstractTableModel>
#include "ModelInterface.h"
#include <QJsonObject>
#include <QPointer>
#include <QUuid>

class MemberInfoModel;

class ScheduleModel  : public QAbstractTableModel
{
	Q_OBJECT

friend class ScheduleController;
friend class ScheduleComponent;


Q_SIGNALS:
	void editTimeError();

public Q_SLOTS:
	void addTimeSlot(const QString& newTime, const QModelIndex& index, const int first, const int last);
	void addDateSlot(const QString& newDate, const QModelIndex& index, const int first, const int last);
	

public:
	ScheduleModel(QObject *parent);
	~ScheduleModel();

	void setScheduleData(QStringList* times, QStringList* dates, QMap<QString, QMap<QString, bool>>* data);
    void bindMember(MemberInfoModel* model, const QUuid& id);
	//void toggleState(const QModelIndex& index);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    bool removeRows(int row, int count, const QModelIndex& parent) override;
	bool removeColumns(int column, int count, const QModelIndex& parent = QModelIndex()) override;

	QStringList* getTimesRow() const;

	void updateTimeSlot(int row, const QString& newTime);

	void updateDateSlot(int column, QDate newDate);

	QString getFullDateString(int column) const;

	bool checkConflict(const QJsonObject& importedData) const;
	void mergeSchedule(const QJsonObject& importedData, bool overwrite);

	QJsonObject exportScheduleData() const;

	void toggleStateByString(const QString& dateKey, const QString& timeKey);
	void removeTimeSlotByString(const QString& time);
	void removeDateSlotByString(const QString& date);

	//基于字符串定位和恢复数据
	QMap<QString, bool> getTimeSlotData(const QString& time) const;
	QMap<QString, bool> getDateSlotData(const QString& date) const;
	void restoreTimeSlot(const QString& time, const QMap<QString, bool>& data);
	void restoreDateSlot(const QString& date, const QMap<QString, bool>& data);
	void updateDateSlotByString(const QString& oldDateStr, const QDate& newDate);
	void setCellStateByString(const QString& dateKey, const QString& timeKey, bool state);

private:
	mutable QStringList* _timesPtr{ nullptr };
	mutable QStringList* _datesPtr{ nullptr };
	mutable QMap<QString, QMap<QString, bool>>* _mapPtr{ nullptr };
    QPointer<MemberInfoModel> _memberModel;
    QUuid _boundMemberId;
    bool _usesMemberBinding{ false };
    void resolveScheduleData() const;
	QString getLastDate() const;
	/*void clearCells(const QModelIndexList& indices);*/
};

