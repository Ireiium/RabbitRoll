#include "rconsole.h"
#include <QVBoxLayout>
#include <QDateTime>

RConsole* RConsole::_instance = nullptr;

RConsole* RConsole::instance()
{
    if (!_instance) {
        _instance = new RConsole(nullptr);
    }
    return _instance;
}

bool RConsole::isInstantiated()
{
    return _instance != nullptr;
}

RConsole::RConsole(QWidget* parent)
    : ElaDialog(parent)
{
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    this->setWindowIcon(QIcon(""));
    this->setIsFixedSize(false);
    this->resize(650, 400);
    this->setWindowTitle("RabbitRoll 运行控制台");

    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 30, 15, 15);

    _logDisplay = new ElaPlainTextEdit();
    _logDisplay->setMinimumSize(580, 380);
    _logDisplay->setReadOnly(true);
    _logDisplay->setPlaceholderText("等待任务运行...");

    mainLayout->addSpacing(10);
    mainLayout->addWidget(_logDisplay);
    //this->setCentralWidget(centralWidget);
}

RConsole::~RConsole()
{
    _instance = nullptr;
}

void RConsole::appendLog(const QString& log)
{
    QString timestamp = QDateTime::currentDateTime().toString("[HH:mm:ss] ");
    _logDisplay->appendPlainText(timestamp + log);

    QTextCursor cursor = _logDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    _logDisplay->setTextCursor(cursor);
}

void RConsole::clearLog()
{
    _logDisplay->clear();
}