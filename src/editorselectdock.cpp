#include "editorselectdock.h"
#include "ElaProgressRing.h"
#include "ElaScrollArea.h"
#include "ElaCheckBox.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QStyle>
#include "componentmanager.h"
#include <QTimer>
#include "idlematchservice.h"
#include "editordutycontroller.h"
#include "memberinfocontroller.h"

EditorSelectDock::EditorSelectDock(QWidget* parent)
    : ElaDockWidget(parent)
{
    setWindowTitle("人员勾选");
    setObjectName("EditorSelectDock");

    auto* eComp = ComponentManager::instance()->getComponent<EditorDutyComponent>();
    _eController = eComp->getController();
    auto* mComp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    _mController = mComp->getController();

    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    initUi();

    connect(IdleMatchService::instance(), &IdleMatchService::precalculationFinished, this, [=]() {
        updateList();
        });

    connect(_eController, &EditorDutyController::cellDataChanged, this,
        [this](int row, int col) {
            if (_currentIndex.isValid() &&
                _currentIndex.row() == row &&
                _currentIndex.column() == col) {
                this->updateList();
            }
        });
}

EditorSelectDock::~EditorSelectDock() {}

void EditorSelectDock::initUi()
{
    this->setMinimumWidth(350);

    ElaScrollArea* scrollArea = new ElaScrollArea(this);
    scrollArea->setWidgetResizable(true);

    QWidget* container = new QWidget();
    container->setObjectName("dockContainer");
    container->setStyleSheet("#dockContainer { background: transparent; }");
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    _stack = new QStackedWidget(this);

    QWidget* loadingPage = new QWidget(this);
    QVBoxLayout* loadingLayout = new QVBoxLayout(loadingPage);
    _busyRing = new ElaProgressRing(this);
    _busyRing->setFixedSize(100, 100);
    _busyRing->setIsBusying(true);
    _busyRing->setValueDisplayMode(ElaProgressRingType::ValueDisplayMode::Percent);

    loadingLayout->addStretch();
    loadingLayout->addWidget(_busyRing, 0, Qt::AlignCenter);
    loadingLayout->addStretch();

    QWidget* listPage = new QWidget(this);
    _listLayout = new QVBoxLayout(listPage);
    _listLayout->setContentsMargins(0, 0, 0, 0);
    _listLayout->setSpacing(12);

    _stack->addWidget(loadingPage);
    _stack->addWidget(listPage);

    mainLayout->addWidget(new ElaText("候选人员名单", 16));
    mainLayout->addSpacing(10);
    mainLayout->addWidget(_stack);
    mainLayout->addStretch();

    scrollArea->setWidget(container);
    setWidget(scrollArea);
}

void EditorSelectDock::setTargetCell(const QModelIndex& index, bool isCalculating)
{
    _currentIndex = index;
    if (isCalculating) {
        _busyRing->setIsBusying(true);
        _stack->setCurrentIndex(0);
    }
    else {
        _busyRing->setIsBusying(false);
        updateList();
        _stack->setCurrentIndex(1);
    }

    if (!this->isVisible()) {
        this->show();
    }

    _eController->updateViewHeight();
}

void EditorSelectDock::onCalculationFinished()
{
    if (this->isVisible() && _stack->currentIndex() == 0) {
        updateList();
        _stack->setCurrentIndex(1);
    }
}

void EditorSelectDock::clearListLayout()
{
    if (!_listLayout) return;
    QLayoutItem* item;
    while (_listLayout->count() > 0) {
        item = _listLayout->takeAt(0);
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
}

void EditorSelectDock::updateList()
{
    if (!_currentIndex.isValid() || !_eController) return;
    clearListLayout();

    QStringList availableIds = _currentIndex.data(Qt::UserRole).toStringList();
    QList<QUuid> selectedIds = _eController->getSelectedIds(_currentIndex);

    if (availableIds.isEmpty()) {
        _listLayout->addWidget(new ElaText("该时段暂无匹配人员", 13));
    }
    else {
        for (const QString& idStr : availableIds) {
            QUuid id(idStr);
            QString name = _mController->getNameById(id);
            if (name.isEmpty()) continue;

            bool inCurrentCell = selectedIds.contains(id);
            int globalDuty = _eController->getMemberDutyCount(id);
            int otherCellCount = globalDuty - (inCurrentCell ? 1 : 0);

            ElaCheckBox* checkBox = new ElaCheckBox(name, this);
            checkBox->setTristate(true);

            if (inCurrentCell) {
                checkBox->setCheckState(Qt::Checked);
            }
            else if (otherCellCount > 0) {
                checkBox->setCheckState(Qt::PartiallyChecked);
            }
            else {
                checkBox->setCheckState(Qt::Unchecked);
            }

            connect(checkBox, &ElaCheckBox::clicked, this, [=](bool checked) {
                if (!_currentIndex.isValid() || !_eController) return;
                QList<QUuid> currentIds = _eController->getSelectedIds(_currentIndex);
                if (checked) {
                    if (!currentIds.contains(id)) currentIds << id;
                }
                else {
                    currentIds.removeAll(id);
                }
                _eController->setSelectedIds(_currentIndex, currentIds);
                });

            _listLayout->addWidget(checkBox);
        }
    }
    _listLayout->addStretch();
}