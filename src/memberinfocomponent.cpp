#include "memberinfocomponent.h"
#include "idlematchservice.h"

MemberInfoComponent::MemberInfoComponent(QObject *parent)
	: ComponentBase(parent)
{
	_model = new MemberInfoModel(this);
	_view = new MemberInfoView();//parent稍后由Page管理
	_delegate = new MemberInfoDelegate(this);

	_view->setModel(_model);
	_view->setItemDelegate(_delegate);

	_controller = new MemberInfoController(_model, _view, this);

	connect(_model, &MemberInfoModel::dataChanged, IdleMatchService::instance(), &IdleMatchService::onDataChanged);
	connect(_model, &MemberInfoModel::rowsInserted, IdleMatchService::instance(), &IdleMatchService::onDataChanged);
	connect(_model, &MemberInfoModel::rowsRemoved, IdleMatchService::instance(), &IdleMatchService::onDataChanged);;
    connect(_model, &MemberInfoModel::modelReset, IdleMatchService::instance(), &IdleMatchService::onDataChanged);
}

MemberInfoComponent::~MemberInfoComponent()
{
}

MemberInfoModel* MemberInfoComponent::getModel() const {
	return _model;
}

MemberInfoView* MemberInfoComponent::getView() const {
	return _view;
}

MemberInfoController* MemberInfoComponent::getController() const {
	return _controller;
}
