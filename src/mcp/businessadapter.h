#pragma once
#include <QJsonObject>
#include <QObject>
#include <memory>
class RabbitRollCommands;
// Application boundary shared by the local IPC transport and in-process tests.
class RabbitRollBusinessAdapter : public QObject {
public:
    explicit RabbitRollBusinessAdapter(QObject* parent = nullptr);
    ~RabbitRollBusinessAdapter() override;
    QJsonObject execute(const QString& operation, const QJsonObject& arguments);
    void cancelPending();
private:
    std::unique_ptr<RabbitRollCommands> commands;
};
