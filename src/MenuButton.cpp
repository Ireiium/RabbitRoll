#include "MenuButton.h"
#include "ElaTheme.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QLabel>

MenuButton::MenuButton(QWidget* parent)
    : QPushButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 10, 20, 10);
    mainLayout->setSpacing(2);

    _titleText = new QLabel(this);
    _titleText->setAttribute(Qt::WA_TransparentForMouseEvents);
    _titleText->setStyleSheet("background: transparent; border: none;");

    _descText = new QLabel(this);
    _descText->setAttribute(Qt::WA_TransparentForMouseEvents);
    _descText->setWordWrap(true);
    _descText->setStyleSheet("background: transparent; border: none;");

    QGraphicsOpacityEffect* opacityEffect = new QGraphicsOpacityEffect(_descText);
    opacityEffect->setOpacity(0.7);
    _descText->setGraphicsEffect(opacityEffect);

    mainLayout->addWidget(_titleText);
    mainLayout->addWidget(_descText);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto updateStyle = [=](ElaThemeType::ThemeMode themeMode) {
        QString colorStr = (themeMode == ElaThemeType::Dark) ? "white" : "black";
        _titleText->setStyleSheet(QString("color: %1; font-size: 18px; background: transparent;").arg(colorStr));
        _descText->setStyleSheet(QString("color: %1; font-size: 16px; background: transparent;").arg(colorStr));
        update();
        };

    connect(eTheme, &ElaTheme::themeModeChanged, this, updateStyle);

    updateStyle(eTheme->getThemeMode());
}

void MenuButton::setTexts(QString title, QString description)
{
    _titleText->setText(QString("<b>%1</b>").arg(title));
    _descText->setText(description);
}

void MenuButton::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    bool isDark = (eTheme->getThemeMode() == ElaThemeType::Dark);
    QColor baseColor = isDark ? QColor(255, 255, 255) : QColor(0, 0, 0);

    int alpha = 15;

    if (!isEnabled()) {
        alpha = 5;
    }
    else if (isDown()) {
        alpha = 50;
    }
    else if (underMouse()) {
        alpha = 35;
    }

    baseColor.setAlpha(alpha);
    painter.setPen(Qt::NoPen);
    painter.setBrush(baseColor);
    painter.drawRoundedRect(rect(), 4, 4);
}

void MenuButton::enterEvent(QEnterEvent* event)
{
    QPushButton::enterEvent(event);
    update();
}

void MenuButton::leaveEvent(QEvent* event)
{
    QPushButton::leaveEvent(event);
    update();
}