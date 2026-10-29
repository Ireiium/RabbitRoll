#pragma once
#include "ElaScrollPage.h"

class QGridLayout;
class QButtonGroup;
class QVBoxLayout;
class QWidget;
class WinDialog;
class MemberInfoController;

class DialogPropertyEdit : public ElaScrollPage
{
    Q_OBJECT
public:
    DialogPropertyEdit(QWidget* parent, WinDialog* dlg);
    ~DialogPropertyEdit();

    QGridLayout* _boxLayout = { nullptr };
	QVBoxLayout* _layout = { nullptr };
	QButtonGroup* _group = { nullptr };
    QWidget* _centralWidget = { nullptr };
    MemberInfoController* _controller{ nullptr };
};
