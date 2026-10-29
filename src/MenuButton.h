#pragma once

#include <QPushButton>
#include <QEnterEvent>
#include <QEvent>

class QLabel;
class MenuButton : public QPushButton
{
    Q_OBJECT
public:
    explicit MenuButton(QWidget* parent = nullptr);
    void setTexts(QString title, QString description);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* _titleText;
    QLabel* _descText;

    void enterEvent(QEnterEvent* event) override; 
    void leaveEvent(QEvent* event) override;
};