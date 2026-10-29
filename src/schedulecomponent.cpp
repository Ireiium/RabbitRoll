#include "schedulecomponent.h"
#include "idlematchservice.h"
#include "componentmanager.h"

ScheduleComponent::ScheduleComponent(QObject* parent)
    : ComponentBase(parent)
{
    _model = new ScheduleModel(this);
    _view = new ScheduleView();
    _delegate = new ScheduleDelegate(this);

    _view->setModel(_model);
    _view->setItemDelegate(_delegate);
    _controller = new ScheduleController(_model, _view, this);

    auto* mComponent = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    _controller->setMemberInfoController(mComponent->getController());


    connect(_view, &ScheduleView::doubleClicked, _controller, &ScheduleController::requestToggleState);
    connect(_model, &ScheduleModel::rowsInserted, _view, &ScheduleView::updateViewHeight);
    connect(_model, &ScheduleModel::rowsRemoved, _view, &ScheduleView::updateViewHeight);
    connect(_model, &ScheduleModel::modelReset, _view, &ScheduleView::updateViewHeight);
    connect(_view, &ScheduleView::requestDeleteRow, _controller, &ScheduleController::deleteTimeRow);
    connect(_view, &ScheduleView::requestDeleteColumn, _controller, &ScheduleController::deleteDateColumn);
    connect(_view, &ScheduleView::requestClearCells, _controller, &ScheduleController::handleClearCellsRequest);
    connect(_view, &ScheduleView::requestEditRow, _controller, &ScheduleController::handleEditRowRequest);
    connect(_model, &ScheduleModel::dataChanged, IdleMatchService::instance(), &IdleMatchService::onDataChanged);
    connect(_model, &ScheduleModel::rowsInserted, IdleMatchService::instance(), &IdleMatchService::onDataChanged);
    connect(_model, &ScheduleModel::columnsInserted, IdleMatchService::instance(), &IdleMatchService::onDataChanged);
    connect(_model, &ScheduleModel::modelReset, IdleMatchService::instance(), &IdleMatchService::onDataChanged);
    connect(_view, &ScheduleView::requestUpdateColumnDate, _controller, &ScheduleController::updateColumnDate);
    connect(_view, &ScheduleView::getFullDateString, this, [this](int column) {
        _view->_fullDateStr = _model->getFullDateString(column);
        });
}

ScheduleComponent::~ScheduleComponent()
{
}

ScheduleView* ScheduleComponent::getView() const {
	return _view;
}

ScheduleModel* ScheduleComponent::getModel() const {
	return _model;
}

ScheduleController* ScheduleComponent::getController() const {
    return _controller;
}

