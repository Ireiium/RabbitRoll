#pragma once

#include <QMainWindow>
#include "ElaWindow.h"
#include <QUndoGroup>

class HomePage;
class EditPage;
class MemberPage;
class SettingPage;
class AboutPage;
class WinDialog;
class ProjectPage;
class QThread;
class QAction;
class ProjectActionController;
class MemberInfoController;
class EditorDutyController;
class AIServiceInterface;

class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    MainWindow(bool hasAI, QWidget* parent = nullptr);
    ~MainWindow();

    void removeRecentFile(const QString& filePath);

private:
    void initialModel();
    void initialWindow();
    void initialNavigation();
    void updateWindowTitle();
    bool maybeSave();
    void updateRecentFiles(const QString& filePath);
    void closeEvent(QCloseEvent* event) override;
    void setProjectWorkspaceEnabled(bool enabled);
    void setNavBar();


    HomePage* _homePage{ nullptr };
    EditPage* _editPage{ nullptr };
    MemberPage* _memberPage{ nullptr };
    SettingPage* _settingPage{ nullptr };
    AboutPage* _aboutPage{ nullptr };
    ProjectPage* _projectPage{ nullptr };
    WinDialog* _winDialog{ nullptr };

    QString _settingKey{ "" };
    QString _aboutKey{ "" };
    QString _defaultName;
    QThread* _workerThread{ nullptr };

    ProjectActionController* _actionController{ nullptr };

    QAction* _saveBtn{ nullptr };
    QAction* _saveAsBtn{ nullptr };
    QAction* _exportExcelBtn{ nullptr };
    QAction* _exportFeishuExcelBtn{ nullptr };
    QAction* _exportMemberBtn{ nullptr };
    QAction* _exportMembersExcelBtn{ nullptr };
    QAction* _importMemberBtn{ nullptr };
    QAction* _undoBtn{ nullptr };
    QAction* _redoBtn{ nullptr };

    QUndoGroup* _undoGroup{ nullptr };
	MemberInfoController* _mController{ nullptr };
	EditorDutyController* _eController{ nullptr };

    void initInterface();

    bool _isAIEnabled;//不能宏编译，因为部分页面需要通过这个bool去构造

#ifdef ENABLE_AI_SUPPORT
    AIServiceInterface* _aiProvider{ nullptr };
    bool _aiInitialized = false;
#endif


};
