#pragma once
#include "businessadapter.h"
#include <QLocalServer>
#include <QSet>

class RabbitRollIpcServer : public QObject {
    Q_OBJECT
public:
    explicit RabbitRollIpcServer(QObject* parent = nullptr);
    ~RabbitRollIpcServer() override;
    bool setEnabled(bool enabled);
    QString status() const;
Q_SIGNALS:
    void statusChanged();
private:
    QLocalServer server;
    QSet<QLocalSocket*> clients;
    RabbitRollBusinessAdapter adapter;
    QString error;
    void acceptConnections();
};
