#include "projectpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "ElaText.h"
#include "ElaScrollPageArea.h"
#include "ElaLineEdit.h"
#include "projectsetting.h"
#include "projectmanager.h"
#include <QEvent>
#include "ElaSpinBox.h"
#include "ElaDrawerArea.h"
#include "ElaToggleSwitch.h"
#include "ElaCalendarPicker.h"
#include "ElaMessageBar.h"

ProjectPage::ProjectPage(QWidget *parent)
	: ElaScrollPage(parent)
{
    initialUI();
    reload();


    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, &ProjectPage::reload);

    connect(ProjectManager::instance(), &ProjectManager::saveFinished, this, [=](bool ok) {
        if (ok) reload();
        });

    /*connect(_nameEdit, &ElaLineEdit::editingFinished, [=]() {
        QString value = _nameEdit->text();
        ProjectSetting::instance()->_projectName = value;
        ProjectManager::instance()->setDirty(true);
        });*/

    connect(_tolSpin, &ElaSpinBox::valueChanged, [=](int value) {
        ProjectSetting::instance()->setToleranceValue(value);
        });

    connect(_dateSearchSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked) {
        if (checked) {
            _drawer->expand();
        }
        else {
            _drawer->collapse();
        }
        ProjectSetting::instance()->enableSerachDateRange(checked);
        ProjectManager::instance()->setDirty(true);
        });

    connect(_drawer, &ElaDrawerArea::expandStateChanged, this, [=](bool isExpand) {
        if (_dateSearchSwitch->getIsToggled() != isExpand) {
            _dateSearchSwitch->blockSignals(true);
            _dateSearchSwitch->setIsToggled(isExpand);
            _dateSearchSwitch->blockSignals(false);
        }
        ProjectSetting::instance()->enableSerachDateRange(isExpand);
        });

    auto updateDates = [=]() {
        QDate start = _startDatePicker->getSelectedDate();
        QDate end = _endDatePicker->getSelectedDate();
        if (start > end) {
            _startDatePicker->blockSignals(true);
            _startDatePicker->setSelectedDate(ProjectSetting::instance()->_searchStartDate);
            _startDatePicker->blockSignals(false);

            _endDatePicker->blockSignals(true);
            _endDatePicker->setSelectedDate(ProjectSetting::instance()->_searchEndDate);
            _endDatePicker->blockSignals(false);

            ElaMessageBar::error(ElaMessageBarType::BottomLeft, "添加失败", "时间不合法", 1000, this);
            return;
        }
        ProjectSetting::instance()->setSerachDateRange(start, end);
        };

    connect(_startDatePicker, &ElaCalendarPicker::selectedDateChanged, this, updateDates);
    connect(_endDatePicker, &ElaCalendarPicker::selectedDateChanged, this, updateDates);
}

ProjectPage::~ProjectPage()
{}


void ProjectPage::initialUI(){
    QWidget* container = new QWidget(this);
    container->setWindowTitle("排班表设置");
    setTitleVisible(false);
    this->addCentralWidget(container);
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 20, 20);
    mainLayout->addWidget(new ElaText("排班表设置"));

    ElaScrollPageArea* nameArea = new ElaScrollPageArea(container);
    mainLayout->addWidget(nameArea);
    QHBoxLayout* nameLayout = new QHBoxLayout(nameArea);
    ElaText* nameText = new ElaText("排班表名称");
    nameText->setTextPixelSize(18);
    nameLayout->addWidget(nameText);
    nameLayout->addStretch();
    _nameEdit = new ElaLineEdit(nameArea);
    _nameEdit->setReadOnly(true);
    _nameEdit->setStyleSheet("background-color: transparent; border: none;");
    _nameEdit->setAlignment(Qt::AlignRight);
    _nameEdit->setFocusPolicy(Qt::NoFocus);
    _nameEdit->setContextMenuPolicy(Qt::NoContextMenu);

    nameLayout->addWidget(_nameEdit);
    nameLayout->addSpacing(20);

    ElaScrollPageArea* tolArea = new ElaScrollPageArea(container);
    mainLayout->addWidget(tolArea);
    QHBoxLayout* tolLayout = new QHBoxLayout(tolArea);
    ElaText* tolText = new ElaText("不可值班容忍度");
    tolText->setTextPixelSize(18);
    tolLayout->addWidget(tolText);
    tolLayout->addStretch();
	_tolSpin = new ElaSpinBox();
    _tolSpin->setButtonMode(ElaSpinBoxType::Compact);
    _tolSpin->setRange(0, 5);
	tolLayout->addWidget(_tolSpin);

    _drawer = new ElaDrawerArea(container);
    mainLayout->addWidget(_drawer);
    QWidget* drawerHeader = new QWidget(this);
    QHBoxLayout* headerLayout = new QHBoxLayout(drawerHeader);
    headerLayout->setContentsMargins(5, 5, 5, 5);
    ElaText* headerText = new ElaText(" 启用日期筛选范围", this);
    headerText->setTextPixelSize(18);
    _dateSearchSwitch = new ElaToggleSwitch(this);

    headerLayout->addWidget(headerText);
    headerLayout->addStretch();
    headerLayout->addWidget(_dateSearchSwitch);
    _drawer->setDrawerHeader(drawerHeader);

    QWidget* startRow = new QWidget(this);
    startRow->setFixedHeight(80);
    QHBoxLayout* startLayout = new QHBoxLayout(startRow);
    startLayout->addWidget(new ElaText("搜索起始时间", 15));
    startLayout->addStretch();
    _startDatePicker = new ElaCalendarPicker(this);
    startLayout->addWidget(_startDatePicker);

    QWidget* endRow = new QWidget(this);
    endRow->setFixedHeight(80);
    QHBoxLayout* endLayout = new QHBoxLayout(endRow);
    endLayout->addWidget(new ElaText("搜索截止时间", 15));
    endLayout->addStretch();
    _endDatePicker = new ElaCalendarPicker(this);
    endLayout->addWidget(_endDatePicker);

    startLayout->setContentsMargins(40, 0, 10, 0);
    endLayout->setContentsMargins(40, 0, 10, 0);

    _drawer->addDrawer(startRow);
    _drawer->addDrawer(endRow);
    

    mainLayout->addStretch();
}

//重置（刷新）
void ProjectPage::reload() {
    _nameEdit->blockSignals(true);
    _tolSpin->blockSignals(true);
    _startDatePicker->blockSignals(true);
    _endDatePicker->blockSignals(true);
    _dateSearchSwitch->blockSignals(true);

    _nameEdit->setText(ProjectSetting::instance()->_projectName);
    _tolSpin->setValue(ProjectSetting::instance()->_toleranceValue);

    _startDatePicker->setSelectedDate(ProjectSetting::instance()->_searchStartDate);
    _endDatePicker->setSelectedDate(ProjectSetting::instance()->_searchEndDate);

    bool isEnabled = ProjectSetting::instance()->_enableSerchDateRange;
    _dateSearchSwitch->blockSignals(true);
    _dateSearchSwitch->setIsToggled(isEnabled);
    _dateSearchSwitch->blockSignals(false);

    if (isEnabled) {
        _drawer->expand();
    }
    else {
        _drawer->collapse();
    }

    _nameEdit->blockSignals(false);
    _tolSpin->blockSignals(false);
    _startDatePicker->blockSignals(false);
    _endDatePicker->blockSignals(false);
    _dateSearchSwitch->blockSignals(false);

}


