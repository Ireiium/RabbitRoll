#include "projectworker.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>

ProjectWorker::ProjectWorker(QObject* parent) : QObject(parent) {}

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
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        Q_EMIT error("保存失败，磁盘空间不足或无权写入。");
        return;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson());

    if (file.commit()) {
        Q_EMIT saveDone();
    }
    else {
        Q_EMIT error("写入失败，文件未完整保存。");
    }
}