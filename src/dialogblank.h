#pragma once

#include <QObject>
#include "ElaScrollPage.h"

class DialogBlank : public ElaScrollPage
{
	Q_OBJECT

public:
	DialogBlank(QWidget *parent);
	~DialogBlank();
};

