#include "RecentFileButton.h"
#include "ElaTheme.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QFileInfo>
#include <QDateTime>
#include <QMenu>
#include <QAction>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QDir>
#include "ElaMenu.h"

RecentFileButton::RecentFileButton(const QString& filePath, QWidget* parent)
    : QPushButton(parent), _filePath(filePath)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(70);

    QFileInfo fileInfo(filePath);

    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(15, 10, 15, 10);
    mainLayout->setSpacing(10);

    QVBoxLayout* leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(2);

    _nameLabel = new QLabel(fileInfo.fileName(), this);
    _nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    _nameLabel->setStyleSheet("background: transparent; border: none; font-weight: bold;");

    _pathLabel = new QLabel(fileInfo.absolutePath(), this);
    _pathLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    _pathLabel->setStyleSheet("background: transparent; border: none;");
    QGraphicsOpacityEffect* pathOpacity = new QGraphicsOpacityEffect(_pathLabel);
    pathOpacity->setOpacity(0.6);
    _pathLabel->setGraphicsEffect(pathOpacity);

    leftLayout->addWidget(_nameLabel);
    leftLayout->addWidget(_pathLabel);

    _dateLabel = new QLabel(fileInfo.lastModified().toString("yyyy/MM/dd HH:mm"), this);
    _dateLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    _dateLabel->setStyleSheet("background: transparent; border: none;");
    _dateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QGraphicsOpacityEffect* dateOpacity = new QGraphicsOpacityEffect(_dateLabel);
    dateOpacity->setOpacity(0.5);
    _dateLabel->setGraphicsEffect(dateOpacity);

    mainLayout->addLayout(leftLayout);
    mainLayout->addStretch();
    mainLayout->addWidget(_dateLabel);

    auto updateStyle = [=](ElaThemeType::ThemeMode themeMode) {
        QString colorStr = (themeMode == ElaThemeType::Dark) ? "white" : "black";
        _nameLabel->setStyleSheet(QString("color: %1; font-size: 16px; background: transparent; font-weight: bold;").arg(colorStr));
        _pathLabel->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colorStr));
        _dateLabel->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colorStr));
        update();
        };

    connect(eTheme, &ElaTheme::themeModeChanged, this, updateStyle);
    updateStyle(eTheme->getThemeMode());
}

QString RecentFileButton::getFilePath() const
{
    return _filePath;
}

void RecentFileButton::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    bool isDark = (eTheme->getThemeMode() == ElaThemeType::Dark);
    QColor baseColor = isDark ? QColor(255, 255, 255) : QColor(0, 0, 0);

    int alpha = 0;
    if (!isEnabled()) {
        alpha = 5;
    }
    else if (isDown()) {
        alpha = 30;
    }
    else if (underMouse()) {
        alpha = 15;
    }

    if (alpha > 0) {
        baseColor.setAlpha(alpha);
        painter.setPen(Qt::NoPen);
        painter.setBrush(baseColor);
        painter.drawRoundedRect(rect(), 6, 6);
    }
}

void RecentFileButton::enterEvent(QEnterEvent* event)
{
    QPushButton::enterEvent(event);
    update();
}

void RecentFileButton::leaveEvent(QEvent* event)
{
    QPushButton::leaveEvent(event);
    update();
}

void RecentFileButton::contextMenuEvent(QContextMenuEvent* event)
{
    ElaMenu menu(this);
    QAction* openDirAction = menu.addAction("打开文件所在位置");
    QAction* deleteAction = menu.addAction("从列表中移除");

    QAction* selectedAction = menu.exec(event->globalPos());

    if (selectedAction == openDirAction)
    {
        openFileInLocation(_filePath);
    }
    else if (selectedAction == deleteAction)
    {
        Q_EMIT deleteRequested(_filePath);
    }
}

void RecentFileButton::openFileInLocation(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) return;

    QString absolutePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());

#if defined(Q_OS_WIN)
    QStringList args;
    args << "/select," << absolutePath;
    QProcess::startDetached("explorer", args);

#elif defined(Q_OS_MAC)

#endif
}

