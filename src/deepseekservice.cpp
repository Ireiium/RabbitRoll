#include "deepseekservice.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>

DeepSeekService::DeepSeekService(QObject* parent)
    : QObject(parent)
{
    m_manager = new QNetworkAccessManager(this);
}

QString DeepSeekService::ask(const QString& systemPrompt, const QString& userPrompt,
    const QString& apiKey, const QString& model)
{
    const QString trimmedApiKey = apiKey.trimmed();
    if (trimmedApiKey.isEmpty()) {
        return QStringLiteral("API Error: 未配置 DeepSeek API Key，请设置 RABBITROLL_DEEPSEEK_API_KEY 或 DEEPSEEK_API_KEY 环境变量。");
    }

    QNetworkRequest request(QUrl("https://api.deepseek.com/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + trimmedApiKey).toUtf8());

    QJsonObject systemMsg, userMsg;
    systemMsg["role"] = "system"; systemMsg["content"] = systemPrompt;
    userMsg["role"] = "user"; userMsg["content"] = userPrompt;

    QJsonArray messages; messages.append(systemMsg); messages.append(userMsg);
    QJsonObject body;
    body["model"] = model;
    body["messages"] = messages;
    body["temperature"] = 0.7;
    body["stream"] = false;

    QJsonDocument doc(body);
    QByteArray data = doc.toJson();

    QNetworkReply* reply = m_manager->post(request, data);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        reply->deleteLater();
        return "API Error: " + err;
    }

    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray respData = reply->readAll();
    reply->deleteLater();

    if (httpStatus != 200) {
        QJsonDocument errDoc = QJsonDocument::fromJson(respData);
        if (errDoc.isObject() && errDoc.object().contains("error")) {
            QString errMsg = errDoc.object()["error"].toObject()["message"].toString();
            return "API Error: " + errMsg;
        }
        return QString("HTTP Error %1: %2").arg(httpStatus).arg(QString::fromUtf8(respData));
    }

    QJsonDocument respDoc = QJsonDocument::fromJson(respData);
    QJsonObject respObj = respDoc.object();
    if (respObj.contains("usage")) {
        QJsonObject usage = respObj["usage"].toObject();
        qDebug() << "[DeepSeek API] Token用量:" << usage["prompt_tokens"].toInt()
            << "提示," << usage["completion_tokens"].toInt() << "完成,"
            << usage["total_tokens"].toInt() << "总计";
    }

    QJsonArray choices = respObj["choices"].toArray();
    if (choices.isEmpty()) return "No response";
    return choices[0].toObject()["message"].toObject()["content"].toString();
}

QString DeepSeekService::askStreaming(const QString& systemPrompt, const QString& userPrompt,
    const QString& apiKey, const QString& model, bool enableThinking)
{
    const QString trimmedApiKey = apiKey.trimmed();
    if (trimmedApiKey.isEmpty()) {
        Q_EMIT responseFinished();
        return QStringLiteral("API Error: 未配置 DeepSeek API Key，请设置 RABBITROLL_DEEPSEEK_API_KEY 或 DEEPSEEK_API_KEY 环境变量。");
    }

    QNetworkRequest request(QUrl("https://api.deepseek.com/chat/completions"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + trimmedApiKey).toUtf8());
    request.setTransferTimeout(120000);

    QJsonObject systemMsg, userMsg;
    systemMsg["role"] = "system"; systemMsg["content"] = systemPrompt;
    userMsg["role"] = "user"; userMsg["content"] = userPrompt;

    QJsonArray messages; messages.append(systemMsg); messages.append(userMsg);
    QJsonObject body;
    body["model"] = model;
    body["messages"] = messages;
    if (!enableThinking) {
        body["temperature"] = 0.7;
    }
    body["stream"] = true;

    if (enableThinking) {
        QJsonObject thinking;
        thinking["type"] = "enabled";
        body["thinking"] = thinking;
    }
    else {
        QJsonObject thinking;
        thinking["type"] = "disabled";
        body["thinking"] = thinking;
    }

    QJsonDocument doc(body);
    QByteArray data = doc.toJson();
    QNetworkReply* reply = m_manager->post(request, data);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QString fullContent;
    QByteArray buffer;

    connect(reply, &QNetworkReply::readyRead, this, [&]() {
        buffer.append(reply->readAll());
        while (true) {
            int idxEnd = buffer.indexOf("\n\n");
            if (idxEnd == -1) break;
            QByteArray line = buffer.left(idxEnd);
            buffer = buffer.mid(idxEnd + 2);

            if (line.startsWith("data: ")) {
                line = line.mid(6);
                if (line == "[DONE]") continue;

                QJsonDocument doc = QJsonDocument::fromJson(line);
                QJsonObject obj = doc.object();
                QJsonArray choices = obj["choices"].toArray();
                if (choices.isEmpty()) continue;

                QJsonObject delta = choices[0].toObject()["delta"].toObject();
                QString reasoning = delta["reasoning_content"].toString();
                QString content = delta["content"].toString();
                QString text = reasoning + content;
                if (!text.isEmpty()) {
                    fullContent += text;
                    Q_EMIT responseChunkReady(text);
                }
            }
        }
        });

    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        reply->deleteLater();
        Q_EMIT responseFinished();
        return "API Error: " + err;
    }

    reply->deleteLater();
    Q_EMIT responseFinished();
    return fullContent;
}