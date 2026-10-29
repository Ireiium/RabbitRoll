#pragma once
#include <QObject>
#include <QString>

class AIServiceInterface : public QObject
{
    Q_OBJECT
public:
    static AIServiceInterface* instance();

    QString generateResponse(const QString& prompt);
    QString generateResponseStreaming(const QString& prompt, bool enableThinking = false);

    void setApiKey(const QString& key);
    void setModel(const QString& model);

Q_SIGNALS:
    void aiProgress(const QString& chunk);
    void aiFinished();

private:
    explicit AIServiceInterface(QObject* parent = nullptr);
    QString m_apiKey;
    QString m_model = "deepseek-v4-flash";
};
