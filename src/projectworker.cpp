#include "projectworker.h"
#include "rschfile.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>

ProjectWorker::ProjectWorker(QObject* parent) : QObject(parent) {}

void ProjectWorker::doSaveSchedule(const QString& filePath, const QJsonObject& root) {
    const auto result = RschFile::write(filePath, QJsonDocument(root).toJson());
    if (result) Q_EMIT saveDone();
    else Q_EMIT error(result.message);
}

void ProjectWorker::doLoad(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        Q_EMIT error("无法打开文件，请检查权限。");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) {
        Q_EMIT error("文件已损坏，不是合法的项目文件。");
        return;
    }
    Q_EMIT loadDone(doc.object());
}

void ProjectWorker::doSave(const QString& filePath, const QJsonObject& root) {
    const auto message = writeJsonFile(filePath,root);
    if (message.isEmpty()) Q_EMIT saveDone();
    else Q_EMIT error(message);
}

QString ProjectWorker::writeJsonFile(const QString& filePath, const QJsonObject& root) {
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return "保存失败：" + file.errorString();
    const QByteArray data = QJsonDocument(root).toJson();
    if (file.write(data) != data.size()) {
        const auto message = file.errorString();
        file.cancelWriting();
        return "写入失败：" + message;
    }
    if (!file.commit()) return "提交文件失败：" + file.errorString();
    return {};
}
