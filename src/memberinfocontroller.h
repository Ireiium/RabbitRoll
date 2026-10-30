#pragma once

#include <QObject>
#include "RTypies.h"
#include <QUuid>
#include "idlematchservice.h"
#include <QUndoStack>

class MemberInfoModel;
class MemberInfoView;
class MemberBitSchedule;
class QLayout;
class MemberInfoController  : public QObject
{
	Q_OBJECT

Q_SIGNALS:
    void memberSelected(const QVariantMap& details, int row);
    void requestUpdateSchedule(void* data);
    void propertyListChanged();
    void batchWorkFinished();
    void searchDataReady(const QStringList& names, const QList<QVariantMap>& dataList);

private Q_SLOTS:
    void onBatchMembersWithFormatProcessed(const QList<QVariantMap>& membersData, const QStringList& format);
    void onBatchMembersProcessed(const QStringList& names);
    

public:
	MemberInfoController(MemberInfoModel* model, MemberInfoView* view, QObject* parent = nullptr);
	~MemberInfoController();

    void setupView(QLayout* layout);
    void addMember(const QString& name = "新成员");
    void removeSelectedMembers();

    void updateMemberProperty(int row, const QString& attrName, const QVariant& newValue);
    void updateMemberProperty(QUuid id, const QString& attrName, const QVariant& newValue);

    void updateMemberNote(int row, const QString& newNote);

    void updatePropertyVisibility(const QStringList& selectedList);

    QModelIndex getViewIndex() const ;

    int getCurrentRow() const;

    void removeProperty(const QString& attrName);

    void* getMemberDataPtr(int row);

    void changeViewClickMode(RTypies::View mode);

    QList<QString> getAllPropertyNames() const;
    QVariantMap getPropertyVisibilityMap() const;

    QVariantMap getMemberDetails(int row) const;
    QString getMemberNote(int row) const;

    int addNewProperty(const QString& name);

    QUuid getIdByName(const QString& name) const;

    QList<MemberBitSchedule> getMemberScheduleSnapshot() const;

    QList<QUuid> getAllValidIds() const;

    QString getNameById(const QUuid& id) const;

    QDate calculateEarliestMonday() const;

    void batchAddMembers(const QStringList& members);

    void refreshMemberDutyCount(int row);
    void updateDutyCount(int row, int count);
    int getDutyCount(int row) const;

    void selectAndShowMember(const QUuid& id);//根据id强行选中成员并触发UI更新

    QUndoStack* getUndoStack() const;

    struct SearchItem {
        QString name;
        int row;
    };
    void scrollToRow(int row);
    void getSearchData(QStringList& names, QList<QVariantMap>& data);
    void requestSearchSuggestionsUpdate();

    void updateMemberPropertyDirectly(int row, const QString& attrName, const QVariant& value);
    void insertMembersDirectly(int startIndex, const QList<QVariantMap>& members);
    void removeMembersDirectly(int startIndex, int count);
    void deleteMembersDirectly(const QList<QVariantMap>& removedData);
    void restoreMembersDirectly(const QList<QVariantMap>& removedData);
    void addPropertyDirectly(const QString& propName);
    void removePropertyDirectly(const QString& propName);
    void restorePropertyDirectly(const QString& propName, bool wasVisible, const QMap<QUuid, QString>& cachedData);

    void setEnableScheduling(int row, bool enable);
    void setScheduleFileSource(int row, const QString& source);


private:
    MemberInfoModel* _model;
    MemberInfoView* _view;

    mutable QDate _earliestDateCache;
    mutable bool _isDateCacheDirty{ true };
    QUndoStack* _undoStack{ nullptr };
    bool _searchUpdatePending{ false };


};

