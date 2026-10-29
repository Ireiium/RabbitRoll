#pragma once

#include "ElaScrollPage.h"
class WinDialog;
class ElaSpinBox;
class DialogTimeSelect : public ElaScrollPage
{
	Q_OBJECT



public:
	DialogTimeSelect(QWidget* parent, WinDialog* dlg);
	~DialogTimeSelect();
	void hideEvent(QHideEvent* event) override;

	void setTime(int h1, int m1, int h2, int m2);

private:
    //问就是现在再去改成hour1就麻烦了所以别动了
	ElaSpinBox* hour_1{ nullptr };
	ElaSpinBox* minute_1{ nullptr };
	ElaSpinBox* hour_2{ nullptr };
	ElaSpinBox* minute_2{ nullptr };
};

