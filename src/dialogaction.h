#pragma once


#include "ElaDialog.h"
#include "RTypies.h"
class DialogAction : public ElaDialog

{
    Q_OBJECT
public:

    DialogAction(QWidget* parent);
    ~DialogAction();

    void setCentralLayout(QLayout* contentLayout);
    void setCentralLayout(QLayout* contentLayout, int width, int height);

};