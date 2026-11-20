#include "singleinstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
constexpr int MaxRequestBytes = 256 * 1024;
constexpr int ForwardTimeoutMs = 10000;
}

SingleInstance::SingleInstance(const QString& directory, QObject* parent)
    : QObject(parent), _directory(directory) {
    if (_directory.isEmpty())
        _directory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QStringLiteral("/Ireiium/RabbitRoll");
    _directory = QDir::cleanPath(QDir(_directory).absolutePath());
    QString identity = _directory;
#ifdef Q_OS_WIN
    identity = identity.toCaseFolded();
#endif
    _serverName = QStringLiteral("RabbitRoll-") + QString::fromLatin1(
        QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex());
    _server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&_server, &QLocalServer::newConnection, this, &SingleInstance::acceptConnections);
}

SingleInstance::StartResult SingleInstance::start(const QString& projectPath, QString* error) {
    if (error) error->clear();
    if (!QDir().mkpath(_directory)) {
        if (error) *error = QStringLiteral("无法创建 RabbitRoll 用户运行目录：%1").arg(_directory);
        return StartResult::Error;
    }
    _lock = std::make_unique<QLockFile>(_directory + QStringLiteral("/instance.lock"));
    // Do not expire a live instance merely because it has been running for a long time.
    _lock->setStaleLockTime(0);
    if (_lock->tryLock()) {
        QLocalServer::removeServer(_serverName);
        if (_server.listen(_serverName)) return StartResult::Primary;
        if (error) *error = QStringLiteral("无法创建 RabbitRoll 本地通信服务：%1").arg(_server.errorString());
        return StartResult::Error;
    }
    if (_lock->error() != QLockFile::LockFailedError) {
        if (error) *error = QStringLiteral("无法锁定 RabbitRoll 用户运行目录：%1").arg(_directory);
        return StartResult::Error;
    }
#ifdef Q_OS_WIN
    qint64 pid = 0;
    QString host, app;
    if (_lock->getLockInfo(&pid, &host, &app) && pid > 0)
        AllowSetForegroundWindow(DWORD(pid));
#endif
    const auto request = QJsonDocument(QJsonObject{{QStringLiteral("path"), projectPath}})
        .toJson(QJsonDocument::Compact) + '\n';
    if (request.size() > MaxRequestBytes) {
        if (error) *error = QStringLiteral("要打开的文件路径过长。");
        return StartResult::Error;
    }
    QElapsedTimer deadline;
    deadline.start();
    // The first process may own the lock before its server is ready.
    QLocalSocket socket;
    while (deadline.elapsed() < ForwardTimeoutMs) {
        socket.connectToServer(_serverName);
        if (socket.waitForConnected(200)) break;
        socket.abort();
        QThread::msleep(50);
    }
    if (socket.state() == QLocalSocket::ConnectedState && socket.write(request) == request.size()) {
        QByteArray reply;
        while (deadline.elapsed() < ForwardTimeoutMs) {
            reply += socket.readAll();
            if (reply == "OK\n") return StartResult::Forwarded;
            if (socket.state() == QLocalSocket::UnconnectedState) break;
            socket.waitForReadyRead(qMax(1, ForwardTimeoutMs - int(deadline.elapsed())));
        }
        reply += socket.readAll();
        if (reply == "OK\n") return StartResult::Forwarded;
    }
    // Never fall back to another editor: that would reintroduce conflicting saves.
    if (error) *error = QStringLiteral("RabbitRoll 已在运行，但未能接收打开请求。请切换到已有窗口处理后重试。");
    return StartResult::Error;
}

void SingleInstance::acceptConnections() {
    while (_server.hasPendingConnections()) {
        auto* socket = _server.nextPendingConnection();
        socket->setParent(this);
        socket->setReadBufferSize(MaxRequestBytes + 1);
        auto* timeout = new QTimer(socket);
        timeout->setSingleShot(true);
        connect(timeout, &QTimer::timeout, socket, &QLocalSocket::abort);
        timeout->start(ForwardTimeoutMs);
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        auto buffer = std::make_shared<QByteArray>();
        auto receive = [this, socket, timeout, buffer] {
            if (socket->property("requestReceived").toBool()) return;
            buffer->append(socket->readAll());
            if (buffer->size() > MaxRequestBytes) {
                socket->abort();
                return;
            }
            if (!buffer->contains('\n')) return;
            const auto document = QJsonDocument::fromJson(*buffer);
            const auto path = document.object().value(QStringLiteral("path"));
            if (!document.isObject() || !path.isString()) {
                socket->abort();
                return;
            }
            socket->setProperty("requestReceived", true);
            timeout->stop();
            socket->write("OK\n");
            socket->disconnectFromServer();
            Q_EMIT openRequested(path.toString());
        };
        connect(socket, &QLocalSocket::readyRead, socket, receive);
        receive();
    }
}
