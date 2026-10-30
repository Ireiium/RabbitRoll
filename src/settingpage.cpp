#include "settingpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "ElaText.h"
#include "ElaDrawerArea.h"
#include "ElaScrollPageArea.h"
#include "ElaComboBox.h"
#include "ElaTheme.h"
#include "ElaRadioButton.h"
#include <QButtonGroup>
#include "ElaApplication.h"
#include "ElaWindow.h"
#include "ElaToggleSwitch.h"
#include <QDir>
#include <QCoreApplication>
#include <QMessageBox>
#include <QStringList>
#include "ElaMessageBar.h"
#include "rconsole.h"
#include "ElaPushButton.h"
#include "scriptengine.h"
#include "settings.h"
#include "exceltemplatepicker.h"

SettingPage::SettingPage(bool hasAI, QWidget* parent)
    : ElaScrollPage(parent)
{
#ifdef ENABLE_AI_SUPPORT
    _isEnabledAI = hasAI;
#endif
    ElaWindow* window = dynamic_cast<ElaWindow*>(parent);
    QWidget* container = new QWidget(this);
    setTitleVisible(false);
    this->addCentralWidget(container);
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 20, 20);
    mainLayout->setSpacing(15);
    mainLayout->addWidget(new ElaText("设置", 30));


    _isEnabledScript = Settings::instance()->getEnableScript();

    mainLayout->addWidget(new ElaText("辅助设置", 20));

    ElaDrawerArea* scriptDrawer = new ElaDrawerArea(container);
    mainLayout->addWidget(scriptDrawer);

    QWidget* scriptHeader = new QWidget(this);
    QHBoxLayout* sHeaderLayout = new QHBoxLayout(scriptHeader);
    sHeaderLayout->setContentsMargins(15, 5, 15, 5);
    ElaText* scriptHeaderText = new ElaText("启用脚本排班", 18);
    _enableScriptSwitch = new ElaToggleSwitch(this);
    sHeaderLayout->addWidget(scriptHeaderText);
    sHeaderLayout->addStretch();
    sHeaderLayout->addWidget(_enableScriptSwitch);
    scriptDrawer->setDrawerHeader(scriptHeader);

    QWidget* scriptRow = new QWidget(this);
    scriptRow->setFixedHeight(50);
    QHBoxLayout* scriptLayout = new QHBoxLayout(scriptRow);
    scriptLayout->setContentsMargins(40, 0, 15, 0);
    ElaText* scriptText = new ElaText("选择脚本", 15);
    ElaComboBox* scriptComboBox = new ElaComboBox(this);
    scriptComboBox->setFixedWidth(250);
    scriptLayout->addWidget(scriptText);
    scriptLayout->addStretch();
    scriptLayout->addWidget(scriptComboBox);
    scriptDrawer->addDrawer(scriptRow);

    QWidget* consoleOpenRow = new QWidget(this);
    consoleOpenRow->setFixedHeight(50);
    QHBoxLayout* consoleOpenLayout = new QHBoxLayout(consoleOpenRow);
    consoleOpenLayout->setContentsMargins(40, 0, 15, 0);
    ElaText* consoleOpenText = new ElaText("打开控制台面板", 15);
    ElaPushButton* consoleOpenBtn = new ElaPushButton("打开", this);
    consoleOpenBtn->setFixedSize(80, 32);
    consoleOpenLayout->addWidget(consoleOpenText);
    consoleOpenLayout->addStretch();
    consoleOpenLayout->addWidget(consoleOpenBtn);
    scriptDrawer->addDrawer(consoleOpenRow);

    QWidget* consoleSwitchRow = new QWidget(this);
    consoleSwitchRow->setFixedHeight(50);
    QHBoxLayout* consoleSwitchLayout = new QHBoxLayout(consoleSwitchRow);
    consoleSwitchLayout->setContentsMargins(40, 0, 15, 0);
    ElaText* consoleText = new ElaText("启用运行控制台(调试)", 15);
    ElaToggleSwitch* consoleSwitch = new ElaToggleSwitch(this);
    consoleSwitchLayout->addWidget(consoleText);
    consoleSwitchLayout->addStretch();
    consoleSwitchLayout->addWidget(consoleSwitch);
    scriptDrawer->addDrawer(consoleSwitchRow);

    bool consoleEnabledSaved = Settings::instance()->getEnableConsole();
    consoleSwitch->setIsToggled(consoleEnabledSaved);

    QDir scriptsDir(QCoreApplication::applicationDirPath() + "/scripts");
    if (!scriptsDir.exists()) {
        scriptsDir.mkpath(".");//如果不存在则创建
    }
    QStringList scripts = scriptsDir.entryList(QStringList() << "*.py", QDir::Files);
    bool scriptEnabledSaved = Settings::instance()->getEnableScript();

    if (scripts.isEmpty()) {
        scriptComboBox->addItem("未找到脚本文件");
        _enableScriptSwitch->setDisabled(true);
        scriptEnabledSaved = false;
    }
    else {
        scriptComboBox->addItems(scripts);
        QString savedScript = Settings::instance()->getDefaultScript();
        _script = savedScript;
        if (!savedScript.isEmpty() && scripts.contains(savedScript)) {
            _script = savedScript;
            scriptComboBox->setCurrentText(savedScript);
        }
        else {
            scriptComboBox->setCurrentIndex(0);
            _script = scriptComboBox->currentText();
            Settings::instance()->setDefaultScript(_script);
        }
    }

    _enableScriptSwitch->blockSignals(true);
    _enableScriptSwitch->setIsToggled(scriptEnabledSaved);
    if (scriptEnabledSaved) {
        scriptDrawer->expand();
    }
    else {
        scriptDrawer->collapse();
    }
    _enableScriptSwitch->blockSignals(false);

#ifdef ENABLE_AI_SUPPORT
    mainLayout->addWidget(new ElaText("辅助功能", 20));
    ElaDrawerArea* aiDrawer = new ElaDrawerArea(container);
    mainLayout->addWidget(aiDrawer);

    QWidget* aiHeader = new QWidget(this);
	aiHeader->setFixedHeight(40);
    QHBoxLayout* aiHeaderLayout = new QHBoxLayout(aiHeader);
    aiHeaderLayout->setContentsMargins(15, 5, 15, 5);
    ElaText* aiHeaderText = new ElaText("启用 AI 排班", 18);
    _enableAISwitch = new ElaToggleSwitch(this);
    aiHeaderLayout->addWidget(aiHeaderText);
    aiHeaderLayout->addStretch();
    aiHeaderLayout->addWidget(_enableAISwitch);
    aiDrawer->setDrawerHeader(aiHeader);

    QWidget* aiContent = new QWidget(this);
    QHBoxLayout* aiContentLayout = new QHBoxLayout(aiContent);
    aiContentLayout->setContentsMargins(40, 0, 15, 0);
    ElaText* modelText = new ElaText("模型：DeepSeek v4 flash", 15);
    modelText->setWordWrap(false);
    aiContentLayout->addWidget(modelText);
    aiContentLayout->addStretch();
    aiDrawer->addDrawer(aiContent);

    bool aiEnabledSaved = Settings::instance()->getEnableAI();

    _enableAISwitch->blockSignals(true);
    _enableAISwitch->setIsToggled(aiEnabledSaved);
    if (aiEnabledSaved) {
        aiDrawer->expand();
    }
    else {
        aiDrawer->collapse();
    }
    _enableAISwitch->blockSignals(false);

    connect(_enableAISwitch, &ElaToggleSwitch::toggled, this, [=](bool checked) {
        Settings::instance()->setEnableAI(checked);
        if (checked) {
            aiDrawer->expand();
        }
        else {
            aiDrawer->collapse();
        }
        ElaMessageBar::success(ElaMessageBarType::BottomLeft, "成功",
            QString("AI 排班已%1").arg(checked ? "启用" : "关闭"), 2000, this);
        Q_EMIT enableAIChanged(checked);
        });

    connect(aiDrawer, &ElaDrawerArea::expandStateChanged, this, [=](bool isExpand) {
        if (_enableAISwitch->getIsToggled() != isExpand) {
            _enableAISwitch->blockSignals(true);
            _enableAISwitch->setIsToggled(isExpand);
            _enableAISwitch->blockSignals(false);

            Settings::instance()->setEnableAI(isExpand);
            ElaMessageBar::success(ElaMessageBarType::BottomLeft, "成功", "设置已更新", 1500, this);
            Q_EMIT enableAIChanged(isExpand);
        }
        });

    Q_EMIT enableAIChanged(_enableAISwitch->getIsToggled());

#endif
    mainLayout->addWidget(new ElaText("导出设置", 20));
    auto* templatePicker = new ExcelTemplatePicker(container);
    mainLayout->addWidget(templatePicker);
    mainLayout->addWidget(new ElaText(" 主题设置", 20));
    ElaScrollPageArea* themeArea = new ElaScrollPageArea(container);
    mainLayout->addWidget(themeArea);
    QHBoxLayout* themeSetting = new QHBoxLayout(themeArea);
    themeSetting->setContentsMargins(15, 5, 15, 5);
    themeSetting->addWidget(new ElaText("外观主题", 18));
    themeSetting->addStretch();

    ElaRadioButton* dayMode = new ElaRadioButton("日间模式", this);
    ElaRadioButton* nightMode = new ElaRadioButton("夜间模式", this);
    QButtonGroup* themeGroup = new QButtonGroup(this);
    themeGroup->addButton(dayMode, 0);
    themeGroup->addButton(nightMode, 1);
    themeSetting->addWidget(dayMode);
    themeSetting->addWidget(nightMode);

    mainLayout->addWidget(new ElaText("窗口设置", 20));

    //窗口效果
    ElaScrollPageArea* winArea = new ElaScrollPageArea(container);
    mainLayout->addWidget(winArea);
    QHBoxLayout* winLayout = new QHBoxLayout(winArea);
    winLayout->setContentsMargins(15, 5, 15, 5);
    winLayout->addWidget(new ElaText("窗口效果", 18));
    winLayout->addStretch();
    ElaRadioButton* normalButton = new ElaRadioButton("Normal", this);
    ElaRadioButton* elaMicaButton = new ElaRadioButton("ElaMica", this);
#ifdef Q_OS_WIN
    ElaRadioButton* micaButton = new ElaRadioButton("Mica", this);
    ElaRadioButton* micaAltButton = new ElaRadioButton("Mica-Alt", this);
    ElaRadioButton* acrylicButton = new ElaRadioButton("Acrylic", this);
#endif
    QButtonGroup* displayButtonGroup = new QButtonGroup(this);
    displayButtonGroup->addButton(normalButton, 0);
    displayButtonGroup->addButton(elaMicaButton, 1);
#ifdef Q_OS_WIN
    displayButtonGroup->addButton(micaButton, 2);
    displayButtonGroup->addButton(micaAltButton, 3);
    displayButtonGroup->addButton(acrylicButton, 4);
#endif
    winLayout->addWidget(normalButton);
    winLayout->addWidget(elaMicaButton);
#ifdef Q_OS_WIN
    winLayout->addWidget(micaButton);
    winLayout->addWidget(micaAltButton);
    winLayout->addWidget(acrylicButton);
#endif

    //导航模式
    // ElaScrollPageArea* navArea = new ElaScrollPageArea(container);
    // mainLayout->addWidget(navArea);
    // QHBoxLayout* navLayout = new QHBoxLayout(navArea);
    // navLayout->addWidget(new ElaText("导航栏模式", 18));
    // navLayout->setContentsMargins(15, 5, 15, 5);
    // navLayout->addStretch();
 //    ElaRadioButton* autoButton = new ElaRadioButton("自动", this);
 //    ElaRadioButton* minButton = new ElaRadioButton("最小化", this);
 //    ElaRadioButton* compactButton = new ElaRadioButton("紧凑", this);
 //    ElaRadioButton* maxButton = new ElaRadioButton("最大化", this);
    // QButtonGroup* navigationGroup = new QButtonGroup(this);
 //    navigationGroup->addButton(autoButton, 0);
 //    navigationGroup->addButton(minButton, 1);
 //    navigationGroup->addButton(compactButton, 2);
 //    navigationGroup->addButton(maxButton, 3);
 //    navLayout->addWidget(autoButton);
 //    navLayout->addWidget(minButton);
 //    navLayout->addWidget(compactButton);
 //    navLayout->addWidget(maxButton);

    QHBoxLayout* recoverLayout = new QHBoxLayout();
    recoverLayout->addStretch();
    ElaPushButton* clearRecentFileBtn = new ElaPushButton("清除最近打开文件列表", this);
    clearRecentFileBtn->setFixedSize(180, 38);
    ElaPushButton* recoverBtn = new ElaPushButton("恢复默认设置", this);
    recoverBtn->setFixedSize(120, 38);
    recoverLayout->addWidget(clearRecentFileBtn);
    recoverLayout->addWidget(recoverBtn);
    mainLayout->addLayout(recoverLayout);

    mainLayout->addStretch();


    //主题恢复
    int themeMode = Settings::instance()->getThemeMode();//默认随主题
    themeGroup->button(themeMode)->setChecked(true);
    eTheme->setThemeMode(themeMode == 0 ? ElaThemeType::Light : ElaThemeType::Dark);

    //窗口模式恢复
    int winMode = Settings::instance()->getWindowMode();//默认normal
    if (displayButtonGroup->button(winMode)) displayButtonGroup->button(winMode)->setChecked(true);
    eApp->setWindowDisplayMode((ElaApplicationType::WindowDisplayMode)winMode);
    Q_EMIT changeWinMode(winMode);

    //导航恢复
    // int navMode = Settings::instance()->getNavMode();//默认紧凑 0-自动 1-最小化 2-紧凑 3-最大化
    // if (navigationGroup->button(navMode)) navigationGroup->button(navMode)->setChecked(true);
    // if (window) window->setNavigationBarDisplayMode((ElaNavigationType::NavigationDisplayMode)navMode);

    connect(_enableScriptSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked) {
        _isEnabledScript = checked;
        if (checked) {
            scriptDrawer->expand();
        }
        else {
            scriptDrawer->collapse();
        }

        //QSettings s("Ireiium", "RabbitRoll");
        //s.setValue("EnableScript", checked);
        Settings::instance()->setEnableScript(checked);

        ElaMessageBar::success(ElaMessageBarType::BottomLeft, "成功", "脚本插件已" + QString(checked ? "启用" : "禁用"), 2000, this);

        Q_EMIT enableScriptChanged(checked, scriptComboBox->currentText());
        });

    auto updateScriptControlsState = [=](bool checked) {
        scriptComboBox->setEnabled(checked);
        consoleOpenBtn->setEnabled(checked);
        consoleSwitch->setEnabled(checked);
        };

    //抽屉展开状态同步开关
    connect(scriptDrawer, &ElaDrawerArea::expandStateChanged, this, [=](bool isExpand) {
        if (_enableScriptSwitch->getIsToggled() != isExpand) {
            _enableScriptSwitch->blockSignals(true);
            _enableScriptSwitch->setIsToggled(isExpand);
            _enableScriptSwitch->blockSignals(false);

            //QSettings s("Ireiium", "RabbitRoll");
            //s.setValue("EnableScript", isExpand);
            Settings::instance()->setEnableScript(isExpand);

            if (_enableScriptSwitch->isEnabled()) {
                ElaMessageBar::success(ElaMessageBarType::BottomLeft, "成功", "设置已更新", 1500, this);
                Q_EMIT enableScriptChanged(isExpand, scriptComboBox->currentText());
            }
        }
        });

    //脚本文件切换
    connect(scriptComboBox, &ElaComboBox::currentTextChanged, this, [=](const QString& text) {
        _script = text;
        if (scripts.contains(text)) {
            //QSettings s("Ireiium", "RabbitRoll");
            //s.setValue("DefaultScript", text);
            Settings::instance()->setDefaultScript(text);

            ElaMessageBar::success(ElaMessageBarType::BottomLeft, "提示", "默认脚本已更换为: " + text, 2000, this);
            Q_EMIT scriptChanged(text);
        }
        });

    connect(consoleOpenBtn, &ElaPushButton::clicked, this, [=]() {
        RConsole::instance()->show();
        RConsole::instance()->raise();
        });

    connect(consoleSwitch, &ElaToggleSwitch::toggled, this, [=](bool checked) {
        //QSettings s("Ireiium", "RabbitRoll");
        //s.setValue("EnableConsole", checked);
        Settings::instance()->setEnableConsole(checked);

        ElaMessageBar::success(ElaMessageBarType::BottomLeft, "提示", checked ? "控制台已启用" : "控制台已关闭", 1500, this);
        });

    updateScriptControlsState(scriptEnabledSaved);

    connect(themeGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
        if (isToggled) {
            int id = themeGroup->id(button);
            //QSettings s("Ireiium", "RabbitRoll");
            //s.setValue("ThemeMode", id);
            Settings::instance()->setThemeMode(id);

            eTheme->setThemeMode(id == 0 ? ElaThemeType::Light : ElaThemeType::Dark);

        }
        });

    connect(eTheme, &ElaTheme::themeModeChanged, this, [=]() {
        if (eTheme->getThemeMode() == ElaThemeType::Light) {
            themeGroup->button(0)->setChecked(true);
            //QSettings s("Ireiium", "RabbitRoll");
            //s.setValue("ThemeMode", 0);
            Settings::instance()->setThemeMode(0);
        }
        else {
            themeGroup->button(1)->setChecked(true);
            //QSettings s("Ireiium", "RabbitRoll");
            //s.setValue("ThemeMode", 1);
            Settings::instance()->setThemeMode(1);
        }
        });


    connect(displayButtonGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
        if (isToggled) {
            int id = displayButtonGroup->id(button);
            //QSettings s("Ireiium", "RabbitRoll");
            //s.setValue("WindowMode", id);
            Settings::instance()->setWindowMode(id);

            eApp->setWindowDisplayMode((ElaApplicationType::WindowDisplayMode)id);
            Q_EMIT changeWinMode(id);
        }
        });

    // connect(navigationGroup, QOverload<QAbstractButton*, bool>::of(&QButtonGroup::buttonToggled), this, [=](QAbstractButton* button, bool isToggled) {
    //     if (isToggled) {
    //         int id = navigationGroup->id(button);
    //         //QSettings s("Ireiium", "RabbitRoll");
    //         //s.setValue("NavMode", id);
    //         Settings::instance()->setNavMode(id);

    //         if (window) window->setNavigationBarDisplayMode((ElaNavigationType::NavigationDisplayMode)id);
    //         Q_EMIT changeWinMode(navigationGroup->id(button));
    //     }
    //     });

    if (ScriptEngine::instance()->_canScriptWork) {
        _enableScriptSwitch->setEnabled(true);
        updateScriptControlsState(true);
    }
    else {
        _enableScriptSwitch->setEnabled(false);
        updateScriptControlsState(false);
    }

    connect(clearRecentFileBtn, &ElaPushButton::clicked, this, [=]() {
        Q_EMIT clearRecentFiles();
        ElaMessageBar::success(ElaMessageBarType::BottomLeft, "成功", "最近打开文件列表已清除", 2000, this);
        });

    connect(recoverBtn, &ElaPushButton::clicked, this, [=]() {
        Settings::instance()->clearAllSettings();
        templatePicker->reloadTemplates();

        if (themeGroup->button(0)) {
            themeGroup->button(0)->setChecked(true);
        }
        if (displayButtonGroup->button(0)) {
            displayButtonGroup->button(0)->setChecked(true);
        }
        // if (navigationGroup->button(2)) {
        //     navigationGroup->button(2)->setChecked(true);
        // }

        _enableScriptSwitch->setIsToggled(false);
        consoleSwitch->setIsToggled(false);

        if (scriptComboBox->count() > 0) {
            scriptComboBox->setCurrentIndex(0);
            QString defaultScript = scriptComboBox->itemText(0);
            Settings::instance()->setDefaultScript(defaultScript);
            _script = defaultScript;
        }


        Q_EMIT enableScriptChanged(false, _script);
        Q_EMIT scriptChanged(_script);

#ifdef ENABLE_AI_SUPPORT
        if (_enableAISwitch)
            _enableAISwitch->setIsToggled(false);
#endif


        Settings::instance()->sync();

        ElaMessageBar::success(ElaMessageBarType::BottomLeft, "成功", "设置已恢复默认", 2000, this);
        });
}

SettingPage::~SettingPage() {}

bool SettingPage::getIsScriptMode() const
{

    if(!_enableScriptSwitch->isEnabled()){
        return false;
    }
    return _isEnabledScript;
}

QString SettingPage::getScript() const
{
    return _script;
}
