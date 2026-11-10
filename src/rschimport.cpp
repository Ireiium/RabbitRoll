#include "rschimport.h"
#include "rschfile.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTime>
#include <algorithm>

QJsonObject RschImport::schedule(const QJsonObject& json, QDate startDate) {
    QJsonObject schedule;
    const auto validTimeRange = [](const QString& value) {
        const QStringList parts = value.split('-');
        if (parts.size() != 2) return false;
        const QTime start = QTime::fromString(parts[0], "HH:mm");
        const QTime end = QTime::fromString(parts[1], "HH:mm");
        return start.isValid() && end.isValid() && start < end;
    };
    QString dataType = json["dataType"].toString();
    if (dataType == "course_schedule") {
        QString school = json["school"].toString();
        if (school == "jwxt.wit.edu.cn" && startDate.isValid() && startDate.dayOfWeek() == 1) {
            struct TimeBlock {
                QString timeStr;
                QList<int> sections;
            };
            QList<TimeBlock> allBlocks;

            for (const QJsonValue& val : json["timetable"].toArray()) {
                QJsonObject obj = val.toObject();
                QString secStr = obj["section"].toVariant().toString();
                QString timeStr = QString("%1-%2").arg(
                    obj["startTime"].toString(), obj["endTime"].toString());
                if (!validTimeRange(timeStr)) continue;

                QList<int> pList;
                if (obj["sections"].isArray()) {
                    for (const auto& section : obj["sections"].toArray()) {
                        if (section.toInt() > 0 && section.toInt() <= 30) pList.append(section.toInt());
                    }
                }
                else if (secStr.length() == 1) {
                    pList << secStr.toInt();
                }
                else if (secStr.length() == 2 && secStr != "10" && secStr != "11") {
                    pList << secStr.left(1).toInt() << secStr.right(1).toInt();
                }
                else if (secStr == "10" || secStr == "11") {
                    pList << secStr.toInt();
                }
                else {
                    for (int i = 0; i + 1 < secStr.length(); i += 2) {
                        pList << secStr.mid(i, 2).toInt();
                    }
                }
                allBlocks.append({ timeStr, pList });
            }

            for (const QJsonValue& val : json["courses"].toArray()) {
                QJsonObject course = val.toObject();
                QList<int> cList;
                for (const QJsonValue& s : course["sections"].toArray()) {
                    cList.append(s.toInt());
                }

                QStringList matchedTime;
                for (const TimeBlock& block : allBlocks) {
                    bool isIntersect = false;
                    for (int cSec : cList) {
                        if (block.sections.contains(cSec)) {
                            isIntersect = true;
                            break;
                        }
                    }
                    if (isIntersect) {
                        matchedTime.append(block.timeStr);
                    }
                }

                if (matchedTime.isEmpty())
                    continue;

                int dayOfWeek = course["dayOfWeek"].toInt();
                if (dayOfWeek < 1 || dayOfWeek > 7) continue;
                for (const QJsonValue& w : course["weeks"].toArray()) {
                    if (w.toInt() < 1 || w.toInt() > 64) continue;
                    QString dateStr = startDate
                        .addDays((w.toInt() - 1) * 7 + (dayOfWeek - 1))
                        .toString(Qt::ISODate);
                    QJsonObject dateObj = schedule[dateStr].toObject();

                    for (const QString& timeStr : matchedTime) {
                        dateObj[timeStr] = true;
                    }
                    schedule[dateStr] = dateObj;
                }
            }
        }
    }
    else if (dataType == "schedule") {
        QJsonObject scheduleData = json["scheduleData"].toObject();
        for (auto it = scheduleData.begin(); it != scheduleData.end(); ++it) {
            if (!QDate::fromString(it.key(), Qt::ISODate).isValid() || !it.value().isObject()) continue;
            QJsonObject times;
            const QJsonObject source = it.value().toObject();
            for (auto time = source.begin(); time != source.end(); ++time) {
                if (validTimeRange(time.key()) && time.value().isBool()) times[time.key()] = time.value();
            }
            if (!times.isEmpty()) schedule[it.key()] = times;
        }
    }

    return schedule;
}

namespace RschImport {
QList<Source> read(const QStringList& paths) {
    QList<Source> result;
    qint64 totalBytes = 0;
    for (const auto& path : paths) {
        Source source;
        source.path = path;
        const QFileInfo file(path);
        totalBytes += file.size();
        if (file.suffix().compare("rsch", Qt::CaseInsensitive) != 0)
            source.error = "请选择 .rsch 时间表文件";
        else if (file.size() > 64 * 1024 * 1024 + 44 || totalBytes > 128 * 1024 * 1024)
            source.error = "文件过大，请分批导入（单文件最多 64 MiB，每批最多 128 MiB）";
        else {
            const auto decoded = RschFile::read(path);
            source.root = decoded.root;
            source.error = decoded.message;
        }
        result.append(source);
    }
    return result;
}

QList<Row> preview(const QList<Source>& files,
    const QList<MemberInfoModel::MemberData>& members, const Options& options) {
    const auto fields = options.format.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QString formatError;
    QSet<QString> unique;
    for (const auto& field : fields) {
        if (field.size() > 8 || field == "序号" || field.startsWith('_'))
            formatError = "属性名最多 8 个字，不能使用序号或以下划线开头";
        if (unique.contains(field)) formatError = "文件名格式中不能重复填写同一属性";
        unique.insert(field);
    }
    if (fields.count("姓名") != 1) formatError = "文件名格式必须包含一个“姓名”，例如：部门 姓名";
    QList<Row> rows;
    QMap<QString, QList<int>> targetRows;
    for (const auto& file : files) {
        Row row;
        row.path = file.path;
        row.canChooseMember = file.error.isEmpty();
        row.error = file.error.isEmpty() ? formatError : file.error;
        const auto values = QFileInfo(file.path).completeBaseName().trimmed()
            .split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (row.error.isEmpty() && values.size() != fields.size())
            row.error = QString("文件名有 %1 段，格式需要 %2 段；可调整格式，或手动选择已有成员仅导入时间表").arg(values.size()).arg(fields.size());
        if (row.error.isEmpty()) {
            for (int i = 0; i < fields.size(); ++i) row.properties.insert(fields[i], values[i]);
        }
        const auto choice = options.targets.value(file.path);
        if (file.error.isEmpty() && !row.error.isEmpty() && !choice.isEmpty() && choice != "new") {
            row.warning = "仅导入时间表：文件名无法按格式解析，成员属性保持原值";
            row.scheduleOnly = true;
            row.error.clear();
        }
        if (row.error.isEmpty()) {
            const auto name = row.properties.value("姓名").toString();
            QList<int> candidates;
            if (!choice.isEmpty() && choice != "new") {
                for (int i = 0; i < members.size(); ++i)
                    if (members[i].id == QUuid(choice)) candidates.append(i);
                if (candidates.isEmpty()) row.error = "所选成员已不存在，请重新选择";
            } else if (choice != "new") {
                for (int i = 0; i < members.size(); ++i)
                    if (members[i].properties.value("姓名").toString().trimmed() == name) candidates.append(i);
                if (candidates.size() > 1 && row.properties.size() > 1) {
                    QList<int> refined;
                    for (const auto index : candidates) {
                        bool matches = true;
                        for (auto p = row.properties.cbegin(); p != row.properties.cend(); ++p)
                            if (p.key() != "姓名" && members[index].properties.value(p.key()).toString().trimmed() != p.value().toString()) matches = false;
                        if (matches) refined.append(index);
                    }
                    if (!refined.isEmpty()) candidates = refined;
                }
            }
            if (row.error.isEmpty() && candidates.size() > 1) row.error = "有重名成员，请在“对应成员”中选择";
            if (row.error.isEmpty() && candidates.isEmpty() && options.existingOnly)
                row.error = "没有找到已有成员，请手动选择，或切换为允许新建";
            if (row.error.isEmpty()) {
                row.create = candidates.isEmpty();
                if (row.create) {
                    row.result.id = QUuid::createUuid();
                    row.result.properties = row.properties;
                } else {
                    row.result = members[candidates.first()];
                    row.targetId = row.result.id;
                    QStringList conflicts;
                    for (auto p = row.properties.cbegin(); p != row.properties.cend(); ++p) {
                        if (p.key() == "姓名") continue;
                        const auto old = row.result.properties.value(p.key()).toString();
                        if (old.trimmed().isEmpty() || options.overwriteProperties) row.result.properties[p.key()] = p.value();
                        else if (old != p.value().toString()) conflicts.append(p.key());
                    }
                    if (!conflicts.isEmpty()) row.warning = "保留已有属性：" + conflicts.join("、");
                }
                if (file.root["dataType"].toString() == "course_schedule"
                    && (!options.semesterStart.isValid() || options.semesterStart.dayOfWeek() != 1))
                    row.error = "请在上方选择学期第一周的周一";
                else {
                    const auto imported = schedule(file.root, options.semesterStart);
                    if (imported.isEmpty()) row.error = "文件中没有可导入的有效时间段";
                    else {
                        if (options.replaceSchedule) {
                            row.result.scheduleDates.clear();
                            row.result.scheduleTimes.clear();
                            row.result.scheduleMap.clear();
                        }
                        for (auto day = imported.begin(); day != imported.end(); ++day) {
                            if (!row.result.scheduleDates.contains(day.key())) row.result.scheduleDates.append(day.key());
                            const auto times = day.value().toObject();
                            for (auto time = times.begin(); time != times.end(); ++time) {
                                if (!row.result.scheduleTimes.contains(time.key())) row.result.scheduleTimes.append(time.key());
                                auto& busy = row.result.scheduleMap[day.key()][time.key()];
                                busy = busy || time.value().toBool();
                            }
                        }
                        row.result.scheduleDates.sort();
                        row.result.scheduleTimes.sort();
                        for (const auto& date : row.result.scheduleDates)
                            for (const auto& time : row.result.scheduleTimes)
                                if (!row.result.scheduleMap[date].contains(time)) row.result.scheduleMap[date][time] = false;
                        const auto dates = imported.keys();
                        row.dateRange = dates.first() == dates.last() ? dates.first() : dates.first() + " 至 " + dates.last();
                        row.result.scheduleFileSource = QFileInfo(file.path).fileName();
                    }
                }
            }
        }
        row.selected = row.error.isEmpty() && !options.skipped.contains(file.path);
        if (row.selected) {
            QString key = row.targetId.toString();
            if (row.create) {
                QStringList identity;
                for (auto p = row.properties.cbegin(); p != row.properties.cend(); ++p)
                    identity.append(p.key() + QChar(0x1f) + p.value().toString());
                key = "new:" + identity.join(QChar(0x1e));
            }
            targetRows[key].append(rows.size());
        }
        rows.append(row);
    }
    for (const auto& duplicates : targetRows) {
        if (duplicates.size() < 2) continue;
        for (const auto index : duplicates) rows[index].error = "多份文件对应同一成员，请只勾选一份或重新指定成员";
    }
    return rows;
}
}
