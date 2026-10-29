#pragma once

#include "componentbase.h"
#include "editordutymodel.h"
#include "editordutycontroller.h"
#include "editordutyview.h"

class EditorDutyModel;
class EditorDutyView;
class EditorDutyController;

class EditorDutyComponent : public ComponentBase
{
    Q_OBJECT
public:
    explicit EditorDutyComponent(QObject* parent = nullptr);

    EditorDutyModel* getModel() const;
    EditorDutyView* getView() const;
    EditorDutyController* getController() const;

private:
    EditorDutyModel* _model{ nullptr };
    EditorDutyView* _view{ nullptr };
    EditorDutyController* _controller{ nullptr };
};