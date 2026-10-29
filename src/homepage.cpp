#include "homepage.h"
#include "ElaText.h"
#include <QVBoxLayout>
#include "ElaImageCard.h"
#include <QDebug>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include "ElaTheme.h"
#include "MenuButton.h"
#include "RecentFileButton.h"
#include <QScrollArea>
#include <QFile>
#include <QMessageBox>
#include "ElaScrollArea.h"
#include "settings.h"

HomePage::HomePage(QWidget* parent)
    : ElaScrollPage(parent)
{
    setWindowTitle("主页");
    setTitleVisible(false);
    setContentsMargins(2, 2, 0, 0);

	initialUI();

    loadRecentFiles();
}

void HomePage::initialUI() {
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    _homepageCard = new ElaImageCard(this);
    _homepageCard->setBorderRadius(10);
    _homepageCard->setFixedHeight(340);
    layout->addWidget(_homepageCard);

    connect(eTheme, &ElaTheme::themeModeChanged, [=]() {
        changeImage(_winMode);
        });

    QVBoxLayout* backgroundLayout = new QVBoxLayout(_homepageCard);
    backgroundLayout->setContentsMargins(20, 20, 20, 20);

    ElaText* homeText = new ElaText("欢迎使用RabbitRoll", this);
    homeText->setTextPixelSize(50);
    //QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    //shadow->setBlurRadius(5);//模糊程度
    //shadow->setColor(Qt::black);//阴影颜色
    //shadow->setOffset(2, 2);//阴影偏移位置
    //homeText->setGraphicsEffect(shadow);
    backgroundLayout->addWidget(homeText);
    backgroundLayout->addStretch();

    QHBoxLayout* selectLayout = new QHBoxLayout();
    layout->addLayout(selectLayout, 1);
    QVBoxLayout* leftLayout = new QVBoxLayout();
    QVBoxLayout* rightLayout = new QVBoxLayout();
    selectLayout->addLayout(leftLayout, 1);
    selectLayout->addLayout(rightLayout, 1);

    rightLayout->setAlignment(Qt::AlignTop);
    rightLayout->setContentsMargins(20, 0, 30, 0);

    leftLayout->setAlignment(Qt::AlignTop);
    leftLayout->setContentsMargins(20, 0, 30, 0);

    ElaText* subTitle = new ElaText("打开最近的排班表：", this);
    subTitle->setTextPixelSize(25);
    leftLayout->addWidget(subTitle);
    ElaScrollArea* recentScrollArea = new ElaScrollArea(this);
    recentScrollArea->setWidgetResizable(true);
    recentScrollArea->setStyleSheet("ElaScrollArea { background: transparent; border: none; } " "ElaScrollArea > QWidget > QWidget { background: transparent; }");
    recentScrollArea->setMinimumHeight(200);

    QWidget* recentScrollWidget = new QWidget(recentScrollArea);
    _recentListLayout = new QVBoxLayout(recentScrollWidget);
    _recentListLayout->setContentsMargins(0, 10, 0, 10);
    _recentListLayout->setSpacing(5);
    _recentListLayout->setAlignment(Qt::AlignTop);

    QStringList recentFiles = Settings::instance()->getRecentFiles();

    //for (const QString& filePath : recentFiles) {
    //    RecentFileButton* fileBtn = new RecentFileButton(filePath, recentScrollWidget);
    //    recentListLayout->addWidget(fileBtn);

    //    connect(fileBtn, &QPushButton::clicked, this, [=]() {
    //        if (!QFile::exists(filePath)) {
    //            QMessageBox::warning(this, "文件丢失", "该排班表文件已被移动、重命名或删除！");

    //            QSettings s("Ireiium", "RabbitRoll");
    //            QStringList currentFiles = s.value("RecentFiles").toStringList();
    //            currentFiles.removeAll(filePath);
    //            s.setValue("RecentFiles", currentFiles);

    //            fileBtn->deleteLater();
    //        }
    //        else {
    //            Q_EMIT openRecentFile(filePath);
    //        }
    //        });
    //    connect(fileBtn, &RecentFileButton::deleteRequested, this, &HomePage::deleteRecentFile);
    //}

    recentScrollArea->setWidget(recentScrollWidget);
    leftLayout->addWidget(recentScrollArea, 1);
    leftLayout->setStretchFactor(recentScrollArea, 1);

    ElaText* subTitle2 = new ElaText("开始使用：", this);
    subTitle2->setTextPixelSize(25);
    rightLayout->addWidget(subTitle2);

    MenuButton* createNewBtn = new MenuButton(this);
    createNewBtn->setTexts("创建新值班表", "开始新的排班任务吧！");
    createNewBtn->setFixedHeight(100);
    createNewBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rightLayout->addWidget(createNewBtn);

    MenuButton* openFileBtn = new MenuButton(this);
    openFileBtn->setTexts("打开值班表", "打开值班表文件。");
    openFileBtn->setFixedHeight(100);
    openFileBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rightLayout->addWidget(openFileBtn);

    rightLayout->addStretch();


    this->addCentralWidget(centralWidget);

    connect(createNewBtn, &QPushButton::clicked, this, [=]() {
        Q_EMIT createNew();
        });
    connect(openFileBtn, &QPushButton::clicked, this, [=]() {
        Q_EMIT openFile();
        });
}

bool HomePage::eventFilter(QObject* watched, QEvent* event)
{
    return ElaScrollPage::eventFilter(watched, event);
}

void HomePage::loadRecentFiles() {
    if (!_recentListLayout) return;

    QLayoutItem* child;
    while ((child = _recentListLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    QStringList recentFiles = Settings::instance()->getRecentFiles();

    for (const QString& filePath : recentFiles) {
        RecentFileButton* fileBtn = new RecentFileButton(filePath, this);
        _recentListLayout->addWidget(fileBtn);
        connect(fileBtn, &QPushButton::clicked, this, [=]() {
            if (!QFile::exists(filePath)) {
                QMessageBox::warning(this, "文件丢失", "该文件已被移动或删除！");
                Settings::instance()->removeRecentFile(filePath);
                loadRecentFiles();
            }
            else {
                Q_EMIT openRecentFile(filePath);
            }
            });
        connect(fileBtn, &RecentFileButton::deleteRequested, this, &HomePage::deleteRecentFile);
    }
}

void HomePage::changeImage(int winMode) {
    _winMode = winMode;
    if ((eTheme->getThemeMode() == ElaThemeType::ThemeMode::Light)) {
        if (winMode == 0) {
            _homepageCard->setCardImage(QImage("://res/image/lightImage.png"));
        }
        else if(winMode == 1){
            _homepageCard->setCardImage(QImage("://res/image/ElaMicaLight.png"));
        }
        else if(winMode == 2){
            _homepageCard->setCardImage(QImage("://res/image/lightImage.png"));
        }
        else if(winMode == 3){
            _homepageCard->setCardImage(QImage("://res/image/lightImage.png"));
        }
        else {
            _homepageCard->setCardImage(QImage("://res/image/acryliclight.png"));
        }
    }
    else {
        if (winMode == 0) {
            _homepageCard->setCardImage(QImage(":/res/image/darkImage.png"));
        }
        else if (winMode == 1) {
            _homepageCard->setCardImage(QImage("://res/image/ElaMicaNight.png"));
        }
        else if (winMode == 2) {
            _homepageCard->setCardImage(QImage("://res/image/darkImage.png"));
        }
        else if (winMode == 3) {
            _homepageCard->setCardImage(QImage("://res/image/darkImage.png"));
        }
        else {
            _homepageCard->setCardImage(QImage("://res/image/acrylicdark.png"));
        }
    }
}
