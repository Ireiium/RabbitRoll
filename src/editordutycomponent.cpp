#include "editordutycomponent.h"
#include "idlematchservice.h"
#include <algorithm>

EditorDutyComponent::EditorDutyComponent(QObject* parent)
    : ComponentBase(parent)
{
    _model = new EditorDutyModel(this);
    _view = new EditorDutyView();
    _controller = new EditorDutyController(_model, _view, this);

    _view->setModel(_model);

    connect(_model, &EditorDutyModel::rowsInserted, IdleMatchService::instance(), &IdleMatchService::onDataChanged);
    connect(_model, &EditorDutyModel::modelReset, IdleMatchService::instance(), &IdleMatchService::onDataChanged);

    connect(_view, &EditorDutyView::requestDeleteRow, _controller, &EditorDutyController::deleteTimeRow);
    connect(_view, &EditorDutyView::requestDeleteColumn, _controller, &EditorDutyController::deleteColumn);
    connect(_view, &EditorDutyView::requestEditRow, _controller, &EditorDutyController::handleEditRow);
    connect(_view, &EditorDutyView::requestUpdateColumnDate, _controller, &EditorDutyController::updateColumnDate);
    connect(_view, &EditorDutyView::requestDeleteRows, _controller, &EditorDutyController::deleteTimeRows);
    connect(_view, &EditorDutyView::requestDeleteColumns, _controller, &EditorDutyController::deleteColumns);
    connect(_view, &EditorDutyView::requestClearCells, _controller, &EditorDutyController::handleClearCellsRequest);
    
}

//getMode和getView别删：一般对于UI层只用controller去调取数据，数据层允许用model直接访问数据（不然太麻烦）
EditorDutyModel* EditorDutyComponent::getModel() const 
{ 
    return _model; 
}
EditorDutyView* EditorDutyComponent::getView() const 
{ 
    return _view; 
}
EditorDutyController* EditorDutyComponent::getController() const 
{ 
    return _controller;
}
