#pragma once

#include "memberinfomodel.h"
#include <QDate>
#include <QJsonObject>
#include <QSet>

namespace RschImport {
struct Source {
    QString path;
    QJsonObject root;
    QString error;
};

struct Options {
    QString format = "姓名";
    QDate semesterStart;
    bool existingOnly = false;
    bool replaceSchedule = false;
    bool overwriteProperties = false;
    QMap<QString, QString> targets; // absent: automatic; "new": create; otherwise member UUID
    QSet<QString> skipped;
};

struct Row {
    QString path;
    QVariantMap properties;
    QUuid targetId;
    MemberInfoModel::MemberData result;
    bool create = false;
    bool selected = false;
    bool canChooseMember = false;
    bool scheduleOnly = false;
    QString error;
    QString warning;
    QString dateRange;
};

QList<Source> read(const QStringList& paths);
QJsonObject schedule(const QJsonObject& root, QDate semesterStart);
QList<Row> preview(const QList<Source>& files,
    const QList<MemberInfoModel::MemberData>& members, const Options& options);
}
