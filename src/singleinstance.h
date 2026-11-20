#pragma once

#include <QLocalServer>
#include <QLockFile>
#include <memory>

class SingleInstance : public QObject {
    Q_OBJECT
public:
    enum class StartResult { Primary, Forwarded, Error };
    // An explicit directory lets tests use their own instance without touching the desktop app.
    explicit SingleInstance(const QString& directory = {}, QObject* parent = nullptr);
    StartResult start(const QString& projectPath, QString* error = nullptr);

Q_SIGNALS:
    void openRequested(const QString& projectPath);

private:
    void acceptConnections();
    QString _directory;
    QString _serverName;
    std::unique_ptr<QLockFile> _lock;
    QLocalServer _server;
};
