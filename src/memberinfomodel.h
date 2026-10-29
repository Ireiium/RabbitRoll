#pragma once

#include <QAbstractTableModel>
#include <QStringList>
#include <QUuid>
#include <QMetaType>

class MemberInfoModel : public QAbstractTableModel
{
	Q_OBJECT


friend class MemberInfoView;
friend class IdleMatchService;
friend class MemberInfoController;
friend class ProjectManager;

Q_SIGNALS:
	void propertyListChanged();
	void modelIniRest();

public:
	struct MemberData {
		QUuid id;
		QVariantMap properties;//属性-属性内容
		QString note;//备注
        int dutyCount = 0;//值班次数

		QStringList scheduleDates;//个人时间表的日期（列头）
		QStringList scheduleTimes;//个人时间表的时间段（行头）
		QMap<QString, QMap<QString, bool>> scheduleMap;//个人时间表[日期][时间][是否有事]
		bool enableScheduling = true;//是否参与排班
		QString scheduleFileSource;//个人时间表导入来源文件名
	};

	explicit MemberInfoModel(QObject* parent = nullptr);
	~MemberInfoModel();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;//行
    int columnCount(const QModelIndex& parent = QModelIndex()) const override; //列
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;//成员信息
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override; //标题
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

	//void addMember(const QString& name);
	//void addMembers(const QStringList& names);
	//void addMembersBatch(const QList<QVariantMap>& membersData);

	bool removeRows(int row, int count, const QModelIndex& parent) override;

	bool updateMemberProperty(int row, const QString& attrName, const QVariant& newValue);

	int addNewProperty(const QString& name);
	void removeProperty(const QString& name);

	bool updateMemberNote(int row, QString note);

	void setPropertyVisible(const QString& name, bool visible);
	MemberData* getMemberDataPtr(int row);

	QString getNameById(const QUuid& id) const;
	QUuid getIdByName(const QString& name) const;
	int getRowById(const QUuid& id) const;

	void updateDutyCount(int row, int count);
	int getDutyCount(int row) const;

	void insertMembersAt(const QList<QPair<int, MemberData>>& membersData);
	void restoreProperty(const QString& name, bool visible, const QMap<QUuid, QString>& oldValues);
	MemberData createDefaultMemberData(const QString& name);
	QList<MemberData> createBatchMembersData(const QStringList& names);
	QList<MemberInfoModel::MemberData> createBatchMembersDataWithFormat(const QList<QVariantMap>& membersData);

	void reset();
	void clear();

	QVariantMap getShowProperties() const;

	bool setEnableScheduling(int row, bool enable);
	bool setScheduleFileSource(int row, const QString& source);

protected:


private:
    QList<MemberData> _members;
    QVariantMap _properties;//属性-显示状态
    QList<QString> _visibleColumns;
    QList<QString> _allProperties;//所有属性列表

	QList<QString>getProperties() const;
	QVariantMap getMemberAllData(int row) const;
	QString getNote(int row)const;
};

Q_DECLARE_METATYPE(MemberInfoModel::MemberData)
Q_DECLARE_METATYPE(QList<MemberInfoModel::MemberData>)
