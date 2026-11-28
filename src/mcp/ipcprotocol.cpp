#include "ipcprotocol.h"
#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>

namespace RabbitRollMcp {
QString serverName() {
    const auto identity = QDir::cleanPath(QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation) + "/Ireiium/RabbitRoll").toCaseFolded();
    return "RabbitRoll-business-v1-" + QString::fromLatin1(
        QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex());
}
QJsonObject failure(const QString& code, const QString& message) {
    return {{"ok", false}, {"error", QJsonObject{{"code", code}, {"message", message}}}};
}
}
