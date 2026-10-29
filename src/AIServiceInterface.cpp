#include "AIServiceInterface.h"
#include "deepseekservice.h"

AIServiceInterface* AIServiceInterface::instance()
{
    static AIServiceInterface inst;
    return &inst;
}

AIServiceInterface::AIServiceInterface(QObject* parent) : QObject(parent) {}

void AIServiceInterface::setApiKey(const QString& key) { m_apiKey = key; }
void AIServiceInterface::setModel(const QString& model) { m_model = model; }

QString AIServiceInterface::generateResponse(const QString& prompt)
{
    const QString systemPrompt = "你是专业排班AI。请严格按要求分配代号(M开头)，只输出纯JSON。";
    DeepSeekService service;
    return service.ask(systemPrompt, prompt, m_apiKey, m_model);
}

QString AIServiceInterface::generateResponseStreaming(const QString& prompt, bool enableThinking)
{
    const QString systemPrompt = "你是专业排班AI...";
    DeepSeekService service;
    QObject::connect(&service, &DeepSeekService::responseChunkReady,
        this, &AIServiceInterface::aiProgress, Qt::DirectConnection);
    QObject::connect(&service, &DeepSeekService::responseFinished,
        this, &AIServiceInterface::aiFinished, Qt::DirectConnection);

    return service.askStreaming(systemPrompt, prompt, m_apiKey, m_model, enableThinking);
}