#pragma once
#include <QObject>
#include <atomic>
#include <QNetworkAccessManager>

class DeepSeekService : public QObject
{
    Q_OBJECT
public:
    explicit DeepSeekService(QObject* parent = nullptr);

    QString ask(const QString& systemPrompt, const QString& userPrompt,
        const QString& apiKey, const QString& model);

    QString askStreaming(const QString& systemPrompt, const QString& userPrompt,
        const QString& apiKey, const QString& model,
        bool enableThinking = false, const std::atomic<bool>* cancelled = nullptr);

Q_SIGNALS:
    void responseChunkReady(const QString& chunk);
    void responseFinished();

private:
    QNetworkAccessManager* m_manager;
};