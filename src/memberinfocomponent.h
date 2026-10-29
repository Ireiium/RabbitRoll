#pragma once

#include "ComponentBase.h"
#include "memberinfocontroller.h"
#include "memberinfomodel.h"
#include "memberinfoview.h"
#include "memberinfodelegate.h"

class MemberInfoModel;
class MemberInfoView;
class MemberInfoDelegate;
class MemberInfoController;

class MemberInfoComponent : public ComponentBase {
    Q_OBJECT
public:
    explicit MemberInfoComponent(QObject* parent = nullptr);
    ~MemberInfoComponent();

    MemberInfoModel* getModel() const;
    MemberInfoView* getView() const;
    MemberInfoController* getController() const;

private:
    MemberInfoModel* _model{ nullptr };
    MemberInfoView* _view{ nullptr };
    MemberInfoDelegate* _delegate{ nullptr };
    MemberInfoController* _controller;
};

