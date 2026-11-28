#pragma once
#include <QObject>
#include <QUuid>
#include <QVariant>
#include <QJsonObject>
#include <QHash>
#include <QQueue>
#include <atomic>
#include <memory>

// Stateful application commands and background jobs, independent of MCP.
class RabbitRollCommands : public QObject {
public:
    explicit RabbitRollCommands(QObject* parent = nullptr);
    ~RabbitRollCommands() override;
    static bool handles(const QString& operation);
    QJsonObject execute(const QString& operation, const QJsonObject& arguments);
    void cancelPending();
private:
    struct Job {
        QString id, operation, state = "running";
        QJsonObject guard, result;
        std::atomic<bool> cancelled{false};
    };
    struct Draft { QJsonObject guard; QHash<QString,QList<QUuid>> schedule; QMap<QUuid,QVariantMap> properties; };
    QHash<QString,Draft> drafts;
    QJsonObject extended(const QString& op, const QJsonObject& args);
    QJsonObject startInputJob(const QString& op, const QJsonObject& args);
    QJsonObject commitSchedule(const QHash<QString,QList<QUuid>>& result, const QMap<QUuid,QVariantMap>& properties);
    QHash<QString, std::shared_ptr<Job>> jobs;
    QHash<QString, QPair<QByteArray, QJsonObject>> replies;
    QQueue<QString> requestOrder;
    QString runningJob;
    QJsonObject startOutputJob(const QString& operation, const QJsonObject& args);
    QJsonObject startJob(const QString& operation, const QJsonObject& args);
    QJsonObject applyCommand(const QString& operation, const QJsonObject& args);
};
