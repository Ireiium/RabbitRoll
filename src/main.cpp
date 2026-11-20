#include "mainwindow.h"
#include "ElaApplication.h"
#include "ElaTheme.h"
#include <QApplication>
#include <QFileInfo>
#include <QStringList>
#include <QTimer>
#include <QMessageBox>
#include "ElaMessageBar.h"
#include "fileassociation.h"
#include "singleinstance.h"
#include "ScriptEngine.h"
#include "rconsole.h"
#include "settings.h"


void msgHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    fflush(stderr);

    if (Settings::instance()->getEnableConsole()) {
        if (RConsole::isInstantiated()) {
            QMetaObject::invokeMethod(RConsole::instance(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
        }
    }
}

namespace {
// 资源管理器双击 .rroll 时会把文件路径作为启动参数传入，这里取出第一个 .rroll 路径。
QString projectFileFromArguments(const QStringList& arguments) {
    for (int index = 1; index < arguments.size(); ++index) {
        const QString& argument = arguments.at(index);
        if (argument.isEmpty() || argument.startsWith(QLatin1Char('-'))) continue;
        const QFileInfo fileInfo(argument);
        if (fileInfo.suffix().compare(QStringLiteral("rroll"), Qt::CaseInsensitive) != 0) continue;
        return fileInfo.absoluteFilePath();
    }
    return QString();
}
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    const QString projectFilePath = projectFileFromArguments(a.arguments());
    SingleInstance instance;
    QString startupError;
    const auto startResult = instance.start(projectFilePath, &startupError);
    if (startResult == SingleInstance::StartResult::Forwarded) return 0;
    if (startResult == SingleInstance::StartResult::Error) {
        QMessageBox::critical(nullptr, "无法启动 RabbitRoll", startupError);
        return 1;
    }

    // Some initialization code can process events before MainWindow is ready.
    QStringList startupRequests;
    const auto startupConnection = QObject::connect(&instance, &SingleInstance::openRequested,
        &a, [&startupRequests](const QString& path) { startupRequests.append(path); });

    QString associationError;
    registerRrollFileAssociation(a.applicationFilePath(), &associationError);
    ElaApplication::eApp->init();
    eTheme->setThemeMode(Settings::instance()->getThemeMode() == 0 ? ElaThemeType::Light : ElaThemeType::Dark);

    ScriptEngine::instance()->init();

    bool hasAI = false;
#ifdef ENABLE_AI_SUPPORT
    hasAI = Settings::instance()->getEnableAI();
#endif

    MainWindow w(hasAI);
    QObject::disconnect(startupConnection);
    QObject::connect(&instance, &SingleInstance::openRequested, &w, [&w](const QString& path) {
        if (w.isMinimized()) w.setWindowState(w.windowState() & ~Qt::WindowMinimized);
        w.show();
        auto* target = QApplication::activeModalWidget();
        if (!target) target = &w;
        target->raise();
        target->activateWindow();
        w.openProjectFromFile(path);
    });
    w.show();

    if (!associationError.isEmpty()) {
        QTimer::singleShot(0, &w, [&w, associationError] {
            ElaMessageBar::warning(ElaMessageBarType::BottomLeft, "文件关联未完成", associationError, 8000, &w);
        });
    }
    if (!projectFilePath.isEmpty()) {
        w.openProjectFromFile(projectFilePath);
    }
    for (const auto& path : startupRequests) w.openProjectFromFile(path);

    return a.exec();
}
