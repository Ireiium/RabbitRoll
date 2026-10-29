#pragma once

#include "ElaDialog.h"
#include "RTypies.h"

class Dialog  : public ElaDialog
{
	Q_OBJECT

Q_SIGNALS:
	void certainClicked_newProperty(const QString& value);
	

public:
	Dialog(RTypies::Dialog key, QWidget* parent);
	~Dialog();
};

