#include "AIServiceInterface.h"
#include "deepseekservice.h"

AIServiceInterface* AIServiceInterface::instance()
{
    static AIServiceInterface inst;
    return &inst;
}

AIServiceInterface::AIServiceInterface(QObject* parent) : QObject(parent) {}

void AIServiceInterface::setApiKey(const QString& key) {
    QWriteLocker lock(&m_configurationLock);
    m_apiKey = key;
}
void AIServiceInterface::setModel(const QString& model) {
    QWriteLocker lock(&m_configurationLock);
    m_model = model;
}
bool AIServiceInterface::hasApiKey() const {
    QReadLocker lock(&m_configurationLock);
    return !m_apiKey.isEmpty();
}

QString AIServiceInterface::generateResponse(const QString& prompt)
{
    QString key, model;
    {
        QReadLocker lock(&m_configurationLock);
        key = m_apiKey;
        model = m_model;
    }
    if (key.isEmpty()) return {};
    const QString systemPrompt = "你是专业排班AI。请严格按要求分配代号(M开头)，只输出纯JSON。";
    DeepSeekService service;
    return service.ask(systemPrompt, prompt, key, model);
}

QString AIServiceInterface::generateResponseStreaming(const QString& prompt, bool enableThinking)
{
    QString key, model;
    {
        QReadLocker lock(&m_configurationLock);
        key = m_apiKey;
        model = m_model;
    }
    if (key.isEmpty()) return {};
    const QString systemPrompt = "你是专业排班AI...";
    DeepSeekService service;
    QObject::connect(&service, &DeepSeekService::responseChunkReady,
        this, &AIServiceInterface::aiProgress, Qt::DirectConnection);
    QObject::connect(&service, &DeepSeekService::responseFinished,
        this, &AIServiceInterface::aiFinished, Qt::DirectConnection);

    return service.askStreaming(systemPrompt, prompt, key, model, enableThinking);
}
