#pragma once

#include <QObject>
#include "ElaScrollPage.h"
#include "RTypies.h"

class EditorDutyController;
class ElaPushButton;
class EditorSelectDock;

class AIDialogCloseFilter : public QObject {
public:
    bool isCalculating = false;
    std::function<void()> onAbort;

    explicit AIDialogCloseFilter(QObject* parent = nullptr) : QObject(parent) {}

	bool eventFilter(QObject* watched, QEvent* event) override;
    
};

class EditPage  : public ElaScrollPage
{
	Q_OBJECT

Q_SIGNALS:
	void popWinDialog(RTypies::WinDialog key, QVariant data);

public Q_SLOTS:
	void onCellDoubleClicked(int row, int col);
	
	void changeScriptMode(bool enable,QString script);
	void changeScript(QString script);

#ifdef ENABLE_AI_SUPPORT
	void changeAIMode(bool isAIMode);
#endif

public:
	EditPage(QWidget *parent);
	~EditPage();

#ifdef ENABLE_AI_SUPPORT
	void checkAISchedule(bool enable);
#endif


protected:

private:
	void initialPtr();
	void initialUI();
	void initialSignalAndSlot();
	void showInfoDock();
	void hideEvent(QHideEvent* event);
	void resetBtnText();

	void popScheduleWindow();
#ifdef ENABLE_AI_SUPPORT
	bool _isAIMode{ false };
	void popAIDialog();
	void showPropertySelectionDialog();
	QSet<QString> _selectedAIProperties;
#endif


	EditorDutyController* _controller{ nullptr };
	ElaPushButton* _changeModeBtn{ nullptr };
	ElaPushButton* _addTimeRowBtn{ nullptr };
	ElaPushButton* _addDateColBtn{ nullptr };
	ElaPushButton* _scheduleBtn{ nullptr };
	ElaPushButton* _AIBtn{ nullptr };
	EditorSelectDock* _selectDock{ nullptr };

	bool _isScriptMode{ false };
	QString _script;
};

