#include "dialogtimeselect.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "ElaSpinBox.h"
#include "ElaText.h"
#include "ElaPushButton.h"
#include "windialog.h"

DialogTimeSelect::DialogTimeSelect(QWidget* parent, WinDialog* dlg)
	: ElaScrollPage(parent)
{
    setTitleVisible(false);
    setContentsMargins(2, 2, 0, 0);
    QWidget* centralWidget = new QWidget(this);
    this->addCentralWidget(centralWidget);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    QHBoxLayout* timeSelect = new QHBoxLayout();
    QHBoxLayout* btnLayout = new QHBoxLayout();
    ElaPushButton* certainBtn = new ElaPushButton("确定", this);
    ElaPushButton* cancelBtn = new ElaPushButton("取消", this);

    layout->addStretch();
    layout->addLayout(timeSelect);
    layout->addStretch();
    layout->addLayout(btnLayout);
    layout->addStretch();

    timeSelect->addStretch();
    hour_1 = new ElaSpinBox();
    hour_1->setButtonMode(ElaSpinBoxType::Compact);
    hour_1->setFixedSize(80, 35);
    hour_1->setRange(0, 23);
    timeSelect->addWidget(hour_1);
    ElaText* colon_1 = new ElaText(":");
    timeSelect->addWidget(colon_1);
    minute_1 = new ElaSpinBox();
    minute_1->setFixedSize(80, 35);
    minute_1->setRange(0, 59);
    minute_1->setButtonMode(ElaSpinBoxType::Compact);
    timeSelect->addWidget(minute_1);
    ElaText* line = new ElaText("至");
    line->setTextPixelSize(14);
    timeSelect->addWidget(line);
    hour_2 = new ElaSpinBox();
    hour_2->setFixedSize(80, 35);
    hour_2->setRange(0, 23);
    hour_2->setButtonMode(ElaSpinBoxType::Compact);
    timeSelect->addWidget(hour_2);
    ElaText* colon_2 = new ElaText(":");
    timeSelect->addWidget(colon_2);
    minute_2 = new ElaSpinBox();
    minute_2->setFixedSize(80, 35);
    minute_2->setRange(0, 59);
    minute_2->setButtonMode(ElaSpinBoxType::Compact);
    timeSelect->addWidget(minute_2);
    timeSelect->addStretch();

    btnLayout->addStretch();
    certainBtn->setFixedSize(100, 40);
    btnLayout->addWidget(certainBtn);
    cancelBtn->setFixedSize(100, 40);
    btnLayout->addStretch();
    btnLayout->addWidget(cancelBtn);
    btnLayout->addStretch();


    connect(cancelBtn, &ElaPushButton::clicked, this, [=]() {
        dlg->closeDialog();
        });

    connect(certainBtn, &ElaPushButton::clicked, this, [=]() {
        dlg->timeSelectCertainNews(hour_1->value(), minute_1->value(), hour_2->value(), minute_2->value());
        dlg->closeDialog();
        });
}

DialogTimeSelect::~DialogTimeSelect()
{
}

void DialogTimeSelect::hideEvent(QHideEvent* event)
{
    
    
    ElaScrollPage::hideEvent(event);

}

void DialogTimeSelect::setTime(int h1, int m1, int h2, int m2) {
    hour_1->setValue(0);
    minute_1->setValue(0);
    hour_2->setValue(0);
    minute_1->setValue(0);

    hour_1->setValue(h1);
    minute_1->setValue(m1);
    hour_2->setValue(h2);
    minute_2->setValue(m2);
}

