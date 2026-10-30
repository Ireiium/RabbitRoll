#pragma once


#include "ElaScrollPage.h"
#include "RTypies.h"

class MemberInformationDock;
class ElaMultiSelectComboBox;
class MemberInfoController;
class ScheduleController;
class ElaPushButton;
class ElaLineEdit;
class QStandardItemModel;
class QCompleter;

class MemberPage  : public ElaScrollPage
{
	Q_OBJECT

Q_SIGNALS:
	void showPropertiesDetails(const QVariantMap& details);
	void popWinDialog(const RTypies::WinDialog key, QVariant data);
	void exportMembersRequested();
	void exportSchedule();

public Q_SLOTS:
	void updatePage(RTypies::Page key, QVariant data);
	void setRecoveryBtn(bool isChanged);
	void onRecoverBtnClicked();
    void updateRecoverButtonState();
	void onSearchDataReady(const QStringList& names, const QList<QVariantMap>& dataList);

public:
	MemberPage(QWidget* parent);
	~MemberPage();

	void showAddItem(const QList<QString>& items, ElaMultiSelectComboBox* box);

	void expoertSucess();

	void SearchSuggestions();


private:
	int _curSeeRow = -1;
	int _curSelectRow = -1;
	MemberInformationDock* _infoDock{ nullptr };
    ElaMultiSelectComboBox* _showProBtn{ nullptr };
	MemberInfoController* _controller{ nullptr };
	ScheduleController* _sController{ nullptr };

	void hideEvent(QHideEvent* event);
	void showInfoDock();

	bool _isScriptChangedData = false;
	ElaPushButton* _recoverBtn{ nullptr };
	ElaLineEdit* _searchBox{ nullptr };
    QStandardItemModel* _searchModel{ nullptr };
    QCompleter* _searchCompleter{ nullptr };

};

