#pragma once

#include <QObject>
#include "ElaWindow.h"
#include "RTypies.h"

class ElaToolBar;
class DialogPropertyEdit;
class DialogTimeSelect;
class DialogBlank;
class DialogBatchAdd;
class MemberInfoController;
class ScheduleController;
class EditorDutyController;
class WinDialog  : public ElaWindow
{
	Q_OBJECT

Q_SIGNALS:
	void timeSelectCertain(const int hour1, const int minute1, const int hour2, const int minute2);
	void actionSuccess(RTypies::Page key, QVariant data);//用于暴露给mainwindow，让mainwidnow接受自己处理完数据并转给page
	void showProPage();
	void selectAll();
	void unSelectAll();
	//void updateMSCB();//更新是否展示属性的下拉菜单
	void deleteChecked();
	void batchAddMembersSignal(const QStringList& members);
	void batchAddWithFormatSignal(const QStringList& lines, const QStringList& format);
	

public:
	WinDialog(QWidget* parent);
	~WinDialog();
	void openWinDialog(const RTypies::WinDialog key, QVariant data = QVariant());
	void closeDialog();
	void timeSelectCertainNews(const int hour1, const int minute1, const int hour2, const int minute2);
	void batchAddMembers(const QStringList& members);

private:
	RTypies::WinDialog _key = RTypies::WinDialog::Null;
	void hideEvent(QHideEvent* event) override;
	void showPropertyEditDialog();
	void showSelectTimeDialog();
	void showCreatNewDialog();
	void showBatchAddDialog();
	void cleanAndDeleteToorBar();


	ElaToolBar* _toolBar{ nullptr };
	MemberInfoController* _mController{ nullptr };
	ScheduleController* _sController{ nullptr };
	EditorDutyController* _eController{ nullptr };

	DialogPropertyEdit* _proPage{ nullptr };
	DialogTimeSelect* _timePage{ nullptr };
	DialogBlank* _blankPage{ nullptr };
	DialogBatchAdd* _batchAddPage{ nullptr };
	QString _currentPageKey{ "" };

	int _editIndex = -1;
};

