#pragma once

#include <QObject>
#include "ElaScrollPage.h"
#include "RTypies.h"

class ElaLineEdit;
class ElaSpinBox;
class ElaDrawerArea;
class ElaCalendarPicker;
class ElaToggleSwitch;

class ProjectPage  : public ElaScrollPage
{
	Q_OBJECT

public:
	ProjectPage(QWidget *parent);
	~ProjectPage();

	void reload();

private:
	void initialUI();
	ElaLineEdit* _nameEdit{ nullptr };
	ElaSpinBox* _tolSpin{ nullptr };
	ElaDrawerArea* _drawer{ nullptr };

	ElaToggleSwitch* _dateSearchSwitch{ nullptr };
	ElaCalendarPicker* _startDatePicker{ nullptr };
	ElaCalendarPicker* _endDatePicker{ nullptr };

};

