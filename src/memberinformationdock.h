#pragma once

#include "ElaDockWidget.h"
#include "RTypies.h"
#include <QResizeEvent>
#include <QDate> 
#include <QUuid>

class QVBoxLayout;
class QFormLayout;
class ElaPushButton;
class ElaPlainTextEdit;
class ModelInterface;
class MemberInfoController;
class ScheduleController;
class ElaText;

class MemberInformationDock : public ElaDockWidget
{
	Q_OBJECT

Q_SIGNALS:
	void propertyChanged(const QString& propertyName, const QString& newValue);
	void noteChanged(int row, QString newNote);
	void addNewTimeRowIsnotValid(RTypies::Message key);
	void popWinDialog(const RTypies::WinDialog key);
	void exportSchedule();
	void finishImportSchedule(bool ok);
	void finishExportSchedule(bool ok);

public Q_SLOTS:

private Q_SLOTS:
	void importSchedule();

public:
    int currentMemberRow() const { return _currentRow; }
	MemberInformationDock(QWidget* parent);
	~MemberInformationDock();
	void setMemberDetails(const QVariantMap& details, const int row);


protected:
	bool eventFilter(QObject* watched, QEvent* event) override;
	void showEvent(QShowEvent* event) override;
	void hideEvent(QHideEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;

private:
	QVBoxLayout* _mainLayout{ nullptr };
	QFormLayout* _formLayout{ nullptr };
	MemberInfoController* _controller{ nullptr };
    ScheduleController* _sController{ nullptr };
	ElaPlainTextEdit* _noteEdit{ nullptr };

	ElaPushButton* _addRowBtn{ nullptr };
	ElaPushButton* _addColBtn{ nullptr };
	ElaPushButton* _importBtn{ nullptr };
	ElaPushButton* _exportBtn{ nullptr };
	ElaText* _scheduleFileLabel{ nullptr };

	QDate _lastChosenStartDate;

	int _currentRow = -1;
    QUuid _currentMemberId;

	void initUi();
	void clearLayout();


};

