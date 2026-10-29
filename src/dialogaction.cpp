#include "dialogaction.h"
#include <QVBoxLayout>

DialogAction::DialogAction(QWidget* parent)
    : ElaDialog(parent)

{
    this->setWindowModality(Qt::WindowModal);
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    this->setWindowIcon(QIcon(""));
    this->setIsFixedSize(true);
}


DialogAction::~DialogAction()
{
}


void DialogAction::setCentralLayout(QLayout* contentLayout) {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(contentLayout);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);
    this->adjustSize();
}


void DialogAction::setCentralLayout(QLayout* contentLayout, int width, int height) {
    if (this->layout()) {
        delete this->layout();
    }
    this->setFixedSize(width, height);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(contentLayout);
    mainLayout->setSizeConstraint(QLayout::SetNoConstraint);

}