#pragma once

#include "ElaDialog.h"
#include "RTypies.h"
#include "ElaPlainTextEdit.h"

class RConsole : public ElaDialog
{
    Q_OBJECT

public:
    static RConsole* instance();
    static bool isInstantiated();

    Q_INVOKABLE void appendLog(const QString& log);
    void clearLog();

private:
    explicit RConsole(QWidget* parent = nullptr);
    ~RConsole();

    static RConsole* _instance;
    ElaPlainTextEdit* _logDisplay;
};