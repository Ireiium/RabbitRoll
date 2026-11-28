#pragma once
#include "ipcprotocol.h"
#include <QFuture>
#include <QObject>

// Transport-only client. Never loads a project or runs business logic.
class RabbitRollIpcClient : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    QFuture<QJsonObject> request(const QString& operation, const QJsonObject& arguments);
};
