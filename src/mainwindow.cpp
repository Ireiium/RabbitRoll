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
#include <QApplication>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QDir>
#ifdef ENABLE_MCP_SUPPORT
#include "mcp/ipcserver.h"
#endif

#ifdef ENABLE_AI_SUPPORT
#include "AIServiceInterface.h"
#endif

namespace {
// 取拖入内容中的第一个本地 .rroll 文件；没有时返回空字符串。
QString rrollFileFromMimeData(const QMimeData* mimeData)
{
    if (!mimeData || !mimeData->hasUrls()) return QString();
    for (const QUrl& url : mimeData->urls()) {
        if (!url.isLocalFile()) continue;
        const QString filePath = url.toLocalFile();
        if (QFileInfo(filePath).suffix().compare(QStringLiteral("rroll"), Qt::CaseInsensitive) == 0) {
            return filePath;
        }
    }
    return QString();
}

// 窗口里有些子控件本身接受拖放（例如搜索框），这里统一拦截 .rroll 的拖放，
// 保证文件拖到窗口任意位置都是打开项目，而不是把路径插入到输入框。
class RrollDropFilter : public QObject
{
public:
    explicit RrollDropFilter(MainWindow* window)
        : QObject(window), _window(window) {}

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        const QEvent::Type type = event->type();
        if (type != QEvent::DragEnter && type != QEvent::DragMove && type != QEvent::Drop) {
            return false;
        }
        auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget || widget->window() != _window) return false;

        if (type == QEvent::Drop) {
            auto* dropEvent = static_cast<QDropEvent*>(event);
            const QString filePath = rrollFileFromMimeData(dropEvent->mimeData());
            if (filePath.isEmpty()) return false;
            dropEvent->acceptProposedAction();
            //拖放处理中不显示模态对话框，等拖放结束后的下一轮事件循环再读取
            QTimer::singleShot(0, _window, [window = _window, filePath]() {
                window->openProjectFromFile(filePath);
                });
            return true;
        }

        auto* dragEvent = static_cast<QDragMoveEvent*>(event);
        if (rrollFileFromMimeData(dragEvent->mimeData()).isEmpty()) return false;
        dragEvent->acceptProposedAction();
        return true;
    }

private:
    MainWindow* _window;
};
}

MainWindow::MainWindow(bool hasAI, QWidget* parent)
    : ElaWindow(parent), _isAIEnabled(hasAI)
{
    _actionController = new ProjectActionController(this);
    initialModel();//初始化顺序不能改
#ifdef ENABLE_MCP_SUPPORT
    _mcpServer = new RabbitRollIpcServer(this);
    if (!_mcpServer->setEnabled(true)) qWarning().noquote() << _mcpServer->status();
#endif
	initialWindow();
    initInterface();
    setNavBar();

#ifdef ENABLE_AI_SUPPORT
    connect(_settingPage, &SettingPage::apiKeyChanged, this, [] {
        AIServiceInterface::instance()->setApiKey(Settings::instance()->getAPI());
    });
    connect(_settingPage, &SettingPage::enableAIChanged, this, [this](bool enabled) {
        _isAIEnabled = enabled;
        if (enabled && !_aiInitialized) {
            initInterface();
        }
        });
#endif
}

MainWindow::~MainWindow() {
#ifdef ENABLE_MCP_SUPPORT
    delete _mcpServer;
#endif
    if (_workerThread && _workerThread->isRunning()) {
        _workerThread->quit();
        _workerThread->wait();
    }
    delete ComponentManager::instance();
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
    setAcceptDrops(true);//允许把 .rroll 文件直接拖入窗口打开
    qApp->installEventFilter(new RrollDropFilter(this));

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
    auto openProject = [this]() {
        if (maybeSave()) _actionController->handleOpenProject();
    };
    connect(_homePage, &HomePage::openFile, this, openProject);
    connect(openBtn, &QAction::triggered, this, openProject);

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

    connect(ProjectManager::instance(), &ProjectManager::projectFileSaved, this, [=](const QString& filePath) {
        updateWindowTitle();
        updateRecentFiles(filePath);
        });

    //关于成员列表导出后的UI反馈 (连接 memberPage)
    connect(_memberPage, &MemberPage::exportMembersRequested, _actionController, &ProjectActionController::handleExportMembersData);
    connect(_memberPage, &MemberPage::exportSchedule, _actionController, &ProjectActionController::handleExportSchedule);


    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, [this] {
        initialNavigation(); setProjectWorkspaceEnabled(true);
        _editPage->setEnabled(true); _memberPage->setEnabled(true); _projectPage->setEnabled(true);
        if (!ProjectManager::instance()->currentFilePath().isEmpty()) updateRecentFiles(ProjectManager::instance()->currentFilePath());
    });
    connect(ProjectManager::instance(), &ProjectManager::projectClosed, this, [this] {
        setProjectWorkspaceEnabled(false);
        _editPage->setEnabled(false); _memberPage->setEnabled(false); _projectPage->setEnabled(false);
        navigation(_homePage->property("ElaPageKey").toString());
    });

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
        return _actionController->handleSaveProject() && !ProjectManager::instance()->isDirty();
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

void MainWindow::openProjectFromFile(const QString& filePath) {
    if (filePath.isEmpty()) return;
    // Resolve paths before queuing, and serialize requests across nested modal event loops.
    _pendingOpenRequests.append(QFileInfo(filePath).absoluteFilePath());
    if (_processingOpenRequests) return;
    _processingOpenRequests = true;
    QTimer::singleShot(0, this, &MainWindow::processNextOpenRequest);
}

void MainWindow::processNextOpenRequest() {
    if (_pendingOpenRequests.isEmpty()) {
        _processingOpenRequests = false;
        return;
    }
    if (QApplication::activeModalWidget() || QApplication::activePopupWidget()) {
        QTimer::singleShot(100, this, &MainWindow::processNextOpenRequest);
        return;
    }
    openProjectFileNow(_pendingOpenRequests.takeFirst());
    QTimer::singleShot(0, this, &MainWindow::processNextOpenRequest);
}

void MainWindow::openProjectFileNow(const QString& filePath) {
    const QFileInfo fileInfo(filePath);
    if (fileInfo.suffix().compare(QStringLiteral("rroll"), Qt::CaseInsensitive) != 0) {
        QMessageBox::warning(this, "无法打开", "只能打开 RabbitRoll 项目文件（*.rroll）。");
        return;
    }
    if (!fileInfo.isFile()) {
        QMessageBox::warning(this, "无法打开",
            QString("文件不存在或无法访问：\n%1").arg(QDir::toNativeSeparators(fileInfo.absoluteFilePath())));
        return;
    }

    const QString absolutePath = fileInfo.absoluteFilePath();
    const QString canonicalPath = fileInfo.canonicalFilePath();
    const QString currentPath = ProjectManager::instance()->currentFilePath();
    if (!currentPath.isEmpty()) {
        const QFileInfo currentInfo(currentPath);
        const QString currentCanonicalPath = currentInfo.canonicalFilePath();
        if (currentInfo.isFile() && !currentCanonicalPath.isEmpty()
            && currentCanonicalPath.compare(canonicalPath, Qt::CaseInsensitive) == 0) {
            //同一个文件已经打开时，拖入或再次双击不重复读取，避免无意义的保存提示
            return;
        }
    }

    if (!maybeSave()) return;//项目已修改时先处理未保存内容

    _actionController->handleOpenRecentProject(absolutePath);
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
