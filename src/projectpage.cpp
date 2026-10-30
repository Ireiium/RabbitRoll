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
#include "ElaToolTip.h"
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

namespace {
class ToleranceHoverHelp : public QObject
{
public:
    ToleranceHoverHelp(QWidget* area, ElaToolTip* tip, QWidget* label, QWidget* icon)
        : QObject(area), _area(area), _tip(tip), _targets{label, icon} {
        _window = area->window();
        area->installEventFilter(this);
        _window->installEventFilter(this);
        for (auto* target : _targets) {
            target->setAttribute(Qt::WA_Hover);
            target->setMouseTracking(true);
            target->installEventFilter(this);
        }
        _tracking.setInterval(100);
        connect(&_tracking, &QTimer::timeout, this, [this] { refresh(); });
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::HoverEnter:
        case QEvent::MouseMove:
        case QEvent::HoverMove:
        case QEvent::WindowActivate:
            refresh();
            break;
        case QEvent::Leave:
        case QEvent::HoverLeave:
            if (watched == _targets[0] || watched == _targets[1]) hide();
            else refresh();
            // Recheck after Qt finishes moving between the label and icon.
            QTimer::singleShot(0, this, [this] { refresh(); });
            break;
        case QEvent::Hide:
        case QEvent::WindowDeactivate:
            if (watched == _area || watched == _window) hide();
            break;
        case QEvent::ToolTip:
            refresh();
            return true;
        default:
            break;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void hide() {
        _tracking.stop();
        _tip->hide();
    }
    void refresh() {
        const auto cursor = QCursor::pos();
        bool overTarget = false;
        for (auto* target : _targets)
            if (target->isVisible() && target->rect().contains(target->mapFromGlobal(cursor))) overTarget = true;
        if (!_area->isVisible() || !_window->isActiveWindow() || !overTarget) {
            hide();
            return;
        }
        _tip->adjustSize();
        QPoint position = cursor + QPoint(12, 20);
        auto* screen = QGuiApplication::screenAt(cursor);
        if (!screen) screen = _area->screen();
        const auto bounds = screen->availableGeometry();
        if (position.x() + _tip->width() > bounds.right() + 1)
            position.setX(cursor.x() - _tip->width() - 12);
        if (position.y() + _tip->height() > bounds.bottom() + 1)
            position.setY(cursor.y() - _tip->height() - 12);
        position.setX(qBound(bounds.left(), position.x(), qMax(bounds.left(), bounds.right() - _tip->width() + 1)));
        position.setY(qBound(bounds.top(), position.y(), qMax(bounds.top(), bounds.bottom() - _tip->height() + 1)));
        _tip->move(position);
        _tip->show();
        if (!_tracking.isActive()) _tracking.start();
    }
    QWidget* _area;
    QWidget* _window;
    ElaToolTip* _tip;
    QWidget* _targets[2];
    QTimer _tracking;
};
}

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
    tolArea->setObjectName("toleranceSettingArea");
    tolArea->setAttribute(Qt::WA_Hover);
    tolArea->setMouseTracking(true);
    mainLayout->addWidget(tolArea);
    QHBoxLayout* tolLayout = new QHBoxLayout(tolArea);
    ElaText* tolText = new ElaText("不可值班容忍度");
    tolText->setTextPixelSize(18);
    const QString toleranceHelp =
        "周期模式下，在指定日期范围内，统计成员在相同星期几和时段\n"
        "无法值班的次数。次数不超过设定值时，仍将其视为可值班。\n"
        "例如：设为 2，最多允许 2 次冲突；设为 0，则不允许冲突。\n"
        "日期模式不受此设置影响，仍按当天的实际空闲情况判断。";
    // Keep Ela's themed popup, shared only by the label and information icon.
    auto* toleranceTip = new ElaToolTip;
    toleranceTip->setParent(tolArea, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint
        | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    toleranceTip->setObjectName("toleranceHelpTip");
    toleranceTip->setAttribute(Qt::WA_ShowWithoutActivating);
    toleranceTip->setShowDelayMsec(0);
    toleranceTip->setHideDelayMsec(0);
    toleranceTip->setDisplayMsec(-1);
    auto* toleranceContent = new ElaText(toleranceHelp, 15, toleranceTip);
    toleranceContent->setWordWrap(true);
    toleranceContent->setFixedWidth(480);
    toleranceTip->setCustomWidget(toleranceContent);
    tolLayout->addWidget(tolText);
    auto* tolInfo = new ElaText(tolArea);
    tolInfo->setElaIcon(ElaIconType::CircleInfo);
    tolInfo->setTextPixelSize(16);
    tolInfo->setFixedSize(22, 22);
    tolInfo->setAccessibleName("不可值班容忍度说明");
    tolLayout->addWidget(tolInfo);
    tolLayout->addStretch();
	_tolSpin = new ElaSpinBox();
    _tolSpin->setButtonMode(ElaSpinBoxType::Compact);
    _tolSpin->setRange(0, 5);
	tolLayout->addWidget(_tolSpin);
    new ToleranceHoverHelp(tolArea, toleranceTip, tolText, tolInfo);

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


