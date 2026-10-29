#pragma once

#include <QObject>
#include "ElaScrollPage.h"

class AboutPage  : public ElaScrollPage
{
	Q_OBJECT

public:
	AboutPage(QWidget* parent);
	~AboutPage();
};

