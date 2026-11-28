#pragma once
#include <QJsonObject>
#include <QString>

namespace RabbitRollMcp {
constexpr int ProtocolVersion = 1;
constexpr int TimeoutMs = 5000;
constexpr int MaxRequestBytes = 256 * 1024;
constexpr int MaxResponseBytes = 4 * 1024 * 1024;
QString serverName();
QJsonObject failure(const QString& code, const QString& message);
}
