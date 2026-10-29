#include "mainwindow.h"
#include "homepage.h"
#include "editpage.h"
#include "memberpage.h"
#include "settingpage.h"
#include "aboutpage.h"
#include "projectpage.h"
#include "ElaMenuBar.h"
#include "ElaMenu.h"
#include <QVBoxLayout>
#include "windialog.h"
#include "componentmanager.h"
#include "ElaToolButton.h"
#include "projectmanager.h"
#include "projectsetting.h"
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QProgressDialog>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include "projectactioncontroller.h"
#include "rconsole.h"
#include "ElaNavigationBar.h"
#include "settings.h"
#include "ElaMessageBar.h"
#include "lock.h"

#ifdef ENABLE_AI_SUPPORT
#include "AIServiceInterface.h"
#endif

MainWindow::MainWindow(bool hasAI, QWidget* parent)
    : ElaWindow(parent), _isAIEnabled(hasAI)
{
    _actionController = new ProjectActionController(this);
    initialModel();//初始化顺序不能改
	initialWindow();
    initInterface();
    setNavBar();

#ifdef ENABLE_AI_SUPPORT
    connect(_settingPage, &SettingPage::enableAIChanged, this, [this](bool enabled) {
        _isAIEnabled = enabled;
        if (enabled && !_aiInitialized) {
            initInterface();
        }
        });
#endif
}

MainWindow::~MainWindow() {
    delete ComponentManager::instance();
    if (_workerThread && _workerThread->isRunning()) {
        _workerThread->quit();
        _workerThread->wait();
    }
}

void MainWindow::initInterface() {
#ifdef ENABLE_AI_SUPPORT
    if (_aiInitialized) return;
    AIServiceInterface* aiService = AIServiceInterface::instance();
    QString apiKey = Settings::instance()->getAPI();
    aiService->setApiKey(apiKey);
    aiService->setModel("deepseek-v4-flash");
    _aiInitialized = true;
    qDebug() << "AI已初始化";
#else

#endif
}

void MainWindow::initialModel() {
    ComponentManager::instance()->registerComponent<MemberInfoComponent>(new MemberInfoComponent(this));
    ComponentManager::instance()->registerComponent<ScheduleComponent>(new ScheduleComponent(this));
    ComponentManager::instance()->registerComponent<EditorDutyComponent>(new EditorDutyComponent(this));

    qRegisterMetaType<ExportTableData>("ExportTableData");
    qRegisterMetaType<QList<ExportTableData>>("QList<ExportTableData>");
    qRegisterMetaType<QHash<QString, QStringList>>("QHash<QString, QStringList>");
    qRegisterMetaType<QHash<QString, QList<QUuid>>>("QHash<QString, QList<QUuid>>");
    qRegisterMetaType<QList<MemberInfoModel::MemberData>>("QList<MemberInfoModel::MemberData>");

    _undoGroup = new QUndoGroup(this);
    _eController = ComponentManager::instance()->getComponent<EditorDutyComponent>()->getController();
    _mController = ComponentManager::instance()->getComponent<MemberInfoComponent>()->getController();
    _undoGroup->addStack(_eController->getUndoStack());
    _undoGroup->addStack(_mController->getUndoStack());
}

void MainWindow::initialWindow()
{
    setWindowTitle("RabbitRoll");
    setWindowIcon(QIcon(":/res/icons/icon.png"));

    this->setNavigationBarDisplayMode(ElaNavigationType::Compact);
    this->setNavigationBarWidth(250);
    this->setIsAllowPageOpenInNewWindow(false);


    _workerThread = new QThread(this);
    BackendWorker* worker = BackendWorker::instance();
    worker->moveToThread(_workerThread);
    _workerThread->start();

    _homePage = new HomePage(this);
    this->addPageNode("主页", _homePage, ElaIconType::House);
    _editPage = new EditPage(this);
    _editPage->hide();
#ifdef ENABLE_AI_SUPPORT
    _editPage->checkAISchedule(_isAIEnabled);
#endif
    _memberPage = new MemberPage(this);
    _memberPage->hide();
    _memberPage->setObjectName("成员");
    _projectPage = new ProjectPage(this);
    _projectPage->hide();

    _settingPage = new SettingPage(_isAIEnabled, this);
    addFooterNode("设置", _settingPage, _settingKey, 0, ElaIconType::GearComplex);
    _aboutPage = new AboutPage(this);
    addFooterNode("关于", _aboutPage, _aboutKey, 0, ElaIconType::CircleInfo);

    _editPage->changeScriptMode(_settingPage->getIsScriptMode(), _settingPage->getScript());
    connect(_settingPage, &SettingPage::scriptChanged, _editPage, &EditPage::changeScript);

	connect(_settingPage, &SettingPage::enableScriptChanged, _editPage, &EditPage::changeScriptMode);
    connect(_settingPage, &SettingPage::changeWinMode, _homePage, &HomePage::changeImage);
    _settingPage->changeWinMode(Settings::instance()->getWindowMode());//刷新图片

    _winDialog = new WinDialog(this);
    connect(_memberPage, &MemberPage::popWinDialog, this, [=](const RTypies::WinDialog key, QVariant data) {
        _winDialog->openWinDialog(key, data);
        });

    connect(_editPage, &EditPage::popWinDialog, this, [=](const RTypies::WinDialog key, QVariant data) {
        _winDialog->openWinDialog(key, data);
        });

    connect(_settingPage, &SettingPage::clearRecentFiles, this, [=]() {
        Settings::instance()->setRecentFiles(QStringList());
        if (_homePage) {
            _homePage->loadRecentFiles();
        }
        });

    connect(_winDialog, &WinDialog::batchAddMembersSignal, worker, &BackendWorker::batchMembers);
    connect(_winDialog, &WinDialog::batchAddWithFormatSignal, worker, &BackendWorker::batchMembersWithFormat);

    ElaMenuBar* menuBar = new ElaMenuBar(this);
    /*menuBar->setStyleSheet("ElaMenuBar { background-color: red; border: none; }");*/
    menuBar->setFixedHeight(30);
    QWidget* customWidget = new QWidget(this);
    //两个菜单为175宽，三个菜单为265宽
    customWidget->setFixedWidth(265);
    QVBoxLayout* customLayout = new QVBoxLayout(customWidget);
    customLayout->setContentsMargins(0, 0, 0, 0);
    customLayout->addWidget(menuBar);
    customLayout->addStretch();
    this->setCustomWidget(ElaAppBarType::LeftArea, customWidget);

    ElaMenu* fileMenu = menuBar->addMenu(ElaIconType::Files, "文件");
    fileMenu->setMenuItemHeight(25);
    QAction* createNewBtn = fileMenu->addElaIconAction(ElaIconType::File, "新建");
    QAction* openBtn = fileMenu->addElaIconAction(ElaIconType::FileImport, "打开");
    fileMenu->addSeparator();

    _saveBtn = fileMenu->addElaIconAction(ElaIconType::FileExport, "保存");
    _saveAsBtn = fileMenu->addElaIconAction(ElaIconType::FileExport, "另存为...");

    ElaMenu* buildMenu = menuBar->addMenu(ElaIconType::AtomSimple, "生成");
    buildMenu->setMenuItemHeight(25);
    _exportExcelBtn = buildMenu->addElaIconAction(ElaIconType::FileExcel, "导出值班表为Excel");
    buildMenu->addSeparator();
    _exportMemberBtn = buildMenu->addElaIconAction(ElaIconType::FileArrowUp, "导出成员列表");
    _exportMembersExcelBtn = buildMenu->addElaIconAction(ElaIconType::FileExcel, "导出成员列表为Excel");
    buildMenu->addSeparator();
    _importMemberBtn = buildMenu->addElaIconAction(ElaIconType::ArrowDownToSquare, "导入成员列表");

    ElaMenu* editMenu = menuBar->addMenu(ElaIconType::Aperture, "编辑");
    editMenu->setMenuItemHeight(25);
    _undoBtn = editMenu->addElaIconAction(ElaIconType::ArrowRotateLeft, "撤销");
    _redoBtn = editMenu->addElaIconAction(ElaIconType::ArrowRotateRight, "重做");

    setProjectWorkspaceEnabled(false); 
    
    auto triggerNewMemberAction = [=]() {
        if (!maybeSave()) return;
        ProjectManager::instance()->newProject("未命名排班表");
        initialNavigation();
        setProjectWorkspaceEnabled(true);//激活界面菜单按钮
        this->navigation(_editPage->property("ElaPageKey").toString());
        };

    //根据当前栈的状态，自动禁用/启用撤销重做按钮
    connect(_undoGroup, &QUndoGroup::canUndoChanged, _undoBtn, &QAction::setEnabled);
    connect(_undoGroup, &QUndoGroup::canRedoChanged, _redoBtn, &QAction::setEnabled);

    //执行撤销和重做
    connect(_undoBtn, &QAction::triggered, _undoGroup, &QUndoGroup::undo);
    connect(_redoBtn, &QAction::triggered, _undoGroup, &QUndoGroup::redo);

    connect(this, &MainWindow::navigationNodeClicked, this, [=](ElaNavigationType::NavigationNodeType nodeType, QString nodeKey) {
        if (nodeKey == _editPage->property("ElaPageKey").toString()) {
            _undoGroup->setActiveStack(_eController->getUndoStack());
        }
        else if (nodeKey == _memberPage->property("ElaPageKey").toString()) {
            _undoGroup->setActiveStack(_mController->getUndoStack());
        }
        });

    //新建与打开
    connect(_homePage, &HomePage::createNew, this, triggerNewMemberAction);
    connect(createNewBtn, &QAction::triggered, this, triggerNewMemberAction);
    connect(_homePage, &HomePage::openFile, _actionController, &ProjectActionController::handleOpenProject);
    connect(openBtn, &QAction::triggered, _actionController, &ProjectActionController::handleOpenProject);

    //最近文件
    connect(_homePage, &HomePage::openRecentFile, this, [=](const QString& filePath) {
        if (!maybeSave()) return;
        _actionController->handleOpenRecentProject(filePath);
        });

    connect(_homePage, &HomePage::deleteRecentFile, this, &MainWindow::removeRecentFile);

    //保存与导出
    connect(_saveBtn, &QAction::triggered, _actionController, &ProjectActionController::handleSaveProject);
    connect(_saveAsBtn, &QAction::triggered, _actionController, &ProjectActionController::handleSaveAsProject);
    connect(_exportExcelBtn, &QAction::triggered, _actionController, &ProjectActionController::handleExportScheduleExcel);
    connect(_exportMembersExcelBtn, &QAction::triggered, _actionController, &ProjectActionController::handleExportMembersExcel);
    connect(_exportMemberBtn, &QAction::triggered, _actionController, &ProjectActionController::handleExportMembersData);
    connect(_importMemberBtn, &QAction::triggered, _actionController, &ProjectActionController::handleImportMembersData);

    //连接Controller与MainWindow的回调通信
    connect(_actionController, &ProjectActionController::projectLoadedSuccessfully, this, [=](const QString& filePath) {
        initialNavigation();
        updateWindowTitle();
        updateRecentFiles(filePath);
        setProjectWorkspaceEnabled(true);//读取成功后激活界面菜单
        });

    connect(_actionController, &ProjectActionController::projectSavedSuccessfully, this, [=](const QString& filePath) {
        updateWindowTitle();
        updateRecentFiles(filePath);
        });

    //关于成员列表导出后的UI反馈 (连接 memberPage)
    connect(_memberPage, &MemberPage::exportMembersRequested, _actionController, &ProjectActionController::handleExportMembersData);
    connect(_memberPage, &MemberPage::exportSchedule, _actionController, &ProjectActionController::handleExportSchedule);


    //更新标题状态
    connect(ProjectManager::instance(), &ProjectManager::projectStatusChanged, this, &MainWindow::updateWindowTitle);
    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, &MainWindow::updateWindowTitle);


#ifdef ENABLE_AI_SUPPORT
    //启用AI的时候通知编辑面更新AI按钮
	connect(_settingPage, &SettingPage::enableAIChanged, _editPage, &EditPage::changeAIMode);
#endif

    createNewBtn->setShortcut(QKeySequence::New);//Ctrl+N
    openBtn->setShortcut(QKeySequence::Open);//Ctrl+O
    _saveBtn->setShortcut(QKeySequence::Save);//Ctrl+S
    _saveAsBtn->setShortcut(QKeySequence("Ctrl+Shift+S"));
    _undoBtn->setShortcut(QKeySequence::Undo);//Ctrl+Z
    _redoBtn->setShortcut(QKeySequence::Redo);//Ctrl+Y

    this->setUserInfoCardTitle("Ireiium");
    this->setUserInfoCardSubTitle("https://github.com/Ireiium");
    this->setUserInfoCardVisible(false);

    connect(_actionController, &ProjectActionController::ImportMembersDataSuccessfully, this, [=](){
        this->navigation(_memberPage->property("ElaPageKey").toString());
        QWidget* parent = this->window() ? this->window() : this;
        ElaMessageBar::success(ElaMessageBarType::BottomLeft, "成功", "导入成员列表成功", 2000, parent);
        });
}

void MainWindow::initialNavigation() {
    if (!_editPage->property("ElaPageKey").isValid()) {
        this->addPageNode("编辑", _editPage, ElaIconType::PenToSquare);
        this->addPageNode("成员", _memberPage, ElaIconType::Users);
        this->addPageNode("排班表设置", _projectPage, ElaIconType::Projector);
    }
    this->navigation(_editPage->property("ElaPageKey").toString());
}

void MainWindow::setProjectWorkspaceEnabled(bool enabled) {
    if (_saveBtn) _saveBtn->setEnabled(enabled);
    if (_saveAsBtn) _saveAsBtn->setEnabled(enabled);
    if (_exportExcelBtn) _exportExcelBtn->setEnabled(enabled);
    if (_exportMemberBtn) _exportMemberBtn->setEnabled(enabled);
    if (_exportMembersExcelBtn) _exportMembersExcelBtn->setEnabled(enabled);
    if (_importMemberBtn) _importMemberBtn->setEnabled(enabled);
    if (_undoBtn) _undoBtn->setEnabled(enabled);
    if (_redoBtn) _redoBtn->setEnabled(enabled);
}

void MainWindow::updateWindowTitle() {
    QString title;
    QString path = ProjectManager::instance()->currentFilePath();

    if (!path.isEmpty()) {
        title = QFileInfo(path).fileName();
    }
    else {
        title = ProjectSetting::instance()->_projectName;
        if (title.isEmpty()) title = "未命名项目";
    }
    if (ProjectManager::instance()->isDirty()) {
        title += " *";
    }
    title += " - RabbitRoll";

    this->setWindowTitle(title);
}

bool MainWindow::maybeSave() {
    if (!ProjectManager::instance()->isDirty()) {
        return true;
    }

    auto resBtn = QMessageBox::question(this, "RabbitRoll", "当前项目已修改，是否保存？",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

    if (resBtn == QMessageBox::Yes) {
        return _actionController->handleSaveProject();
    }
    else if (resBtn == QMessageBox::Cancel) {
        return false;
    }

    return true;
}

void MainWindow::updateRecentFiles(const QString& filePath) {
    Settings::instance()->addRecentFile(filePath);

    if (_homePage) {
        _homePage->loadRecentFiles();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) {
        if (RConsole::isInstantiated()) {
            RConsole::instance()->hide();
        }
        event->accept();
        qApp->quit();
    }
    else {
        event->ignore();
    }
}

void MainWindow::removeRecentFile(const QString& filePath) {
    Settings::instance()->removeRecentFile(filePath);
    if (_homePage) {
        _homePage->loadRecentFiles();
    }
}

void MainWindow::setNavBar() {
    ElaNavigationBar* navBar = this->findChild<ElaNavigationBar*>();
    QWidget* suggestBox = navBar->findChild<QWidget*>("", Qt::FindDirectChildrenOnly);
    QList<QWidget*> suggestBoxes = navBar->findChildren<QWidget*>();
    for (auto* widget : suggestBoxes) {
        if (widget->inherits("ElaSuggestBox")) {
            widget->hide();
            widget->setFixedSize(0, 0);
        }
    }
    QList<ElaToolButton*> toolButtons = navBar->findChildren<ElaToolButton*>();
    for (ElaToolButton* btn : toolButtons) {
        btn->hide();
        btn->setFixedSize(0, 0);
        btn->setEnabled(false);
    }
}
