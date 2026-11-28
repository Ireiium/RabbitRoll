#include "ipcclient.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QPromise>
#include <QTimer>
#include <QUuid>
#include <memory>
#include <utility>

QFuture<QJsonObject> RabbitRollIpcClient::request(const QString& operation, const QJsonObject& arguments) {
    using namespace RabbitRollMcp;
    auto promise = std::make_shared<QPromise<QJsonObject>>();
    promise->start();
    auto future = promise->future();
    auto* socket = new QLocalSocket(this);
    socket->setReadBufferSize(MaxResponseBytes + 1);
    auto* timer = new QTimer(socket);
    timer->setSingleShot(true);
    auto done = std::make_shared<bool>(false);
    auto finish = [socket, promise, done](const QJsonObject& result) {
        if (std::exchange(*done, true)) return;
        promise->addResult(result);
        promise->finish();
        socket->abort();
        socket->deleteLater();
    };
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto payload = QJsonDocument(QJsonObject{{"version", ProtocolVersion}, {"id", id},
        {"operation", operation}, {"arguments", arguments},
        {"deadline", double(QDateTime::currentMSecsSinceEpoch() + TimeoutMs)}}).toJson(QJsonDocument::Compact) + '\n';
    connect(timer, &QTimer::timeout, socket, [finish] {
        finish(failure("timeout", "RabbitRoll 未在 5 秒内响应，请检查主窗口是否忙碌后重试。"));
    });
    connect(socket, &QLocalSocket::connected, socket, [socket, payload] { socket->write(payload); });
    auto buffer = std::make_shared<QByteArray>();
    connect(socket, &QLocalSocket::readyRead, socket, [socket, buffer, id, finish] {
        buffer->append(socket->readAll());
        if (buffer->size() > MaxResponseBytes) {
            finish(failure("response_too_large", "RabbitRoll 响应超过限制。"));
            return;
        }
        if (!buffer->contains('\n')) return;
        const auto doc = QJsonDocument::fromJson(*buffer);
        const auto obj = doc.object();
        if (!doc.isObject() || obj["id"] != id || obj["version"].toInt() != ProtocolVersion
            || !obj["result"].isObject()) {
            finish(failure("invalid_response", "RabbitRoll IPC 响应无效。"));
            return;
        }
        finish(obj["result"].toObject());
    });
    connect(socket, &QLocalSocket::errorOccurred, socket, [finish](QLocalSocket::LocalSocketError) {
        finish(failure("unavailable", "请先启动 RabbitRoll；MCP 随程序默认开启，如仍无法连接请检查程序启动日志。"));
    });
    connect(socket, &QLocalSocket::disconnected, socket, [finish] {
        finish(failure("disconnected", "RabbitRoll 本地业务连接已断开，请确认程序正在运行后重试。"));
    });
    if (payload.size() > MaxRequestBytes) {
        finish(failure("request_too_large", "请求超过 256 KiB，请分批操作。"));
        return future;
    }
    timer->start(TimeoutMs);
    socket->connectToServer(serverName());
    return future;
}
