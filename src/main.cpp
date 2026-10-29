#include "mainwindow.h"
#include "ElaApplication.h"
#include <QApplication>
#include "ScriptEngine.h"
#include "rconsole.h"
#include "settings.h"

#ifdef ENABLE_AI_SUPPORT
#include <QDir>
#include <QFile>
#include <QSettings>
#endif


void msgHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    fflush(stderr);

    if (Settings::instance()->getEnableConsole()) {
        if (RConsole::isInstantiated()) {
            QMetaObject::invokeMethod(RConsole::instance(), "appendLog", Qt::QueuedConnection, Q_ARG(QString, msg));
        }
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ElaApplication::eApp->init();

    ScriptEngine::instance()->init();

    bool hasAI = false;
#ifdef ENABLE_AI_SUPPORT
    hasAI = Settings::instance()->getEnableAI();
#endif

    MainWindow w(hasAI);
    w.show();
    return a.exec();
}
