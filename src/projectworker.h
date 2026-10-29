#pragma once

#include <QObject>
#include <QJsonObject>

class ProjectWorker : public QObject
{
    Q_OBJECT
public:
    explicit ProjectWorker(QObject* parent = nullptr);

Q_SIGNALS:
    void loadDone(const QJsonObject& data);
    void saveDone();
    void error(const QString& msg);

public Q_SLOTS:
    void doLoad(const QString& filePath);
    void doSave(const QString& filePath, const QJsonObject& root);
};