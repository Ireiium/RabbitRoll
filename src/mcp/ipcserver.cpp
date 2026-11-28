#include "ipcserver.h"
#include "ipcprotocol.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QTimer>
#include <memory>
#include <qt_windows.h>

namespace {
// The pipe DACL (UserAccessOption) authenticates the Windows user token.
// Reject remote named-pipe clients as well, even when they use that account.
bool localClient(QLocalSocket* socket) {
    ULONG session = 0;
    DWORD ownSession = 0;
    return GetNamedPipeClientSessionId(reinterpret_cast<HANDLE>(socket->socketDescriptor()), &session)
        && ProcessIdToSessionId(GetCurrentProcessId(), &ownSession) && session == ownSession;
}
}
RabbitRollIpcServer::RabbitRollIpcServer(QObject* parent) : QObject(parent) {
    server.setSocketOptions(QLocalServer::UserAccessOption);
    server.setMaxPendingConnections(32);
    connect(&server, &QLocalServer::newConnection, this, &RabbitRollIpcServer::acceptConnections);
}
RabbitRollIpcServer::~RabbitRollIpcServer() { setEnabled(false); }
bool RabbitRollIpcServer::setEnabled(bool enabled) {
    error.clear();
    if (!enabled) {
        adapter.cancelPending();
        server.close();
        const auto sockets = clients;
        for (auto* socket : sockets) socket->abort();
    } else if (!server.isListening()) {
        // Windows permits multiple named-pipe server handles for one name.
        // Refuse an existing endpoint instead of joining it accidentally.
        QLocalSocket probe;
        probe.connectToServer(RabbitRollMcp::serverName());
        if (probe.waitForConnected(50)) error = "本地业务通信名称已被占用。";
        else if (!server.listen(RabbitRollMcp::serverName())) error = server.errorString();
    }
    Q_EMIT statusChanged();
    return error.isEmpty();
}
QString RabbitRollIpcServer::status() const {
    if (!error.isEmpty()) return "MCP 启动失败：" + error;
    return server.isListening() ? QString("MCP 已启用 · 本机当前用户 · 活动请求 %1").arg(clients.size()) : "MCP 已关闭";
}
void RabbitRollIpcServer::acceptConnections() {
    using namespace RabbitRollMcp;
    while (server.hasPendingConnections()) {
        auto* socket = server.nextPendingConnection();
        socket->setParent(this);
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            clients.remove(socket);
            socket->deleteLater();
            Q_EMIT statusChanged();
        });
        if (!server.isListening() || clients.size() >= 32 || !localClient(socket)) { socket->abort(); continue; }
        clients.insert(socket);
        Q_EMIT statusChanged();
        socket->setReadBufferSize(MaxRequestBytes + 1);
        auto* timer = new QTimer(socket);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, socket, &QLocalSocket::abort);
        timer->start(TimeoutMs);
        auto buffer = std::make_shared<QByteArray>();
        auto receive = [this, socket, buffer] {
            if (socket->property("received").toBool()) return;
            if (!server.isListening()) { socket->abort(); return; }
            buffer->append(socket->readAll());
            if (buffer->size() > MaxRequestBytes) { socket->abort(); return; }
            if (!buffer->contains('\n')) return;
            socket->setProperty("received", true);
            const auto doc = QJsonDocument::fromJson(*buffer);
            const auto request = doc.object();
            const auto id = request["id"].toString();
            QJsonObject result;
            if (!doc.isObject() || id.isEmpty() || id.size() > 64 || request["version"].toInt() != ProtocolVersion
                || !request["arguments"].isObject() || !request["operation"].isString()
                || !request["deadline"].isDouble())
                result = failure("invalid_request", "无效的业务 IPC 请求。");
            else if (request["deadline"].toDouble() < QDateTime::currentMSecsSinceEpoch())
                result = failure("timeout", "请求已超时，未执行操作。");
            else result = adapter.execute(request["operation"].toString(), request["arguments"].toObject());
            auto reply = QJsonDocument(QJsonObject{{"version", ProtocolVersion}, {"id", id}, {"result", result}}).toJson(QJsonDocument::Compact) + '\n';
            if (reply.size() > MaxResponseBytes)
                reply = QJsonDocument(QJsonObject{{"version", ProtocolVersion}, {"id", id},
                    {"result", failure("response_too_large", "请缩小查询范围。")}}).toJson(QJsonDocument::Compact) + '\n';
            socket->write(reply);
            socket->disconnectFromServer();
        };
        connect(socket, &QLocalSocket::readyRead, socket, receive);
        receive();
    }
}
