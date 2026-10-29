#pragma once

#include "ComponentBase.h"
#include "schedulecontroller.h"
#include "schedulemodel.h"
#include "scheduleview.h"
#include "scheduledelegate.h"

class ScheduleModel;
class ScheduleView;
class ScheduleDelegate;
class ScheduleController;
class ScheduleComponent  : public ComponentBase
{
	Q_OBJECT
	
public:
	ScheduleComponent(QObject *parent);
	~ScheduleComponent();
	ScheduleModel* getModel() const;
	ScheduleView* getView() const;
	ScheduleController* getController() const;

private:
	ScheduleModel* _model{ nullptr };
	ScheduleView* _view{ nullptr };
	ScheduleDelegate* _delegate{ nullptr };
	ScheduleController* _controller{ nullptr };
};

