#include "backendworker.h"
#include <QJsonArray>
#include <QJsonObject>
#include "xlsxdocument.h"
#include "scriptengine.h"
#include <random>
#include <algorithm> 
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonParseError>

#ifdef ENABLE_AI_SUPPORT
#include "AIServiceInterface.h"
#endif

BackendWorker* BackendWorker::instance() {
    static BackendWorker instance;
    return &instance;
}

BackendWorker::BackendWorker(QObject* parent) 
    : QObject(parent) 
{}

void BackendWorker::batchMembers(const QStringList& members) {
    QStringList vMembers;
    for (const QString& name : members) {
        if (QString trimmed = name.trimmed(); !trimmed.isEmpty()) {
            vMembers << trimmed;
        }
    }
    Q_EMIT batchMembersFinished(vMembers);
}

void BackendWorker::batchMembersWithFormat(const QStringList& lines, const QStringList& format) {
    QList<QVariantMap> membersData;

    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) continue;

        QStringList values = trimmedLine.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        QVariantMap member;

        for (int i = 0; i < format.size(); ++i) {
            QString key = format[i];
            QString val = (i < values.size()) ? values[i] : "";//空字符补全没写的
            member.insert(key, val);
        }

        if (!member.contains("姓名") || member.value("姓名").toString().isEmpty()) {
            continue;
        }

        membersData.append(member);
    }

    Q_EMIT batchMembersWithFormatFinished(membersData, format);
}

void BackendWorker::scheduleImport(const QJsonObject& json, QDate startDate) {
    QJsonObject schedule;
    QString dataType = json["dataType"].toString();
    if (dataType == "course_schedule") {
        QString school = json["school"].toString();
        if (school == "jwxt.wit.edu.cn") {
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

                QList<int> pList;
                if (secStr.length() == 2) {
                    pList << secStr.left(1).toInt() << secStr.right(1).toInt();
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
                for (const QJsonValue& w : course["weeks"].toArray()) {
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
            schedule[it.key()] = it.value().toObject();
        }
    }

    Q_EMIT scheduleImportFinished(schedule);
}

void BackendWorker::exportExcel(const QString& filePath, const QList<ExportTableData>& tablesData) {
    QXlsx::Document xlsx;
    QXlsx::Format header;
    header.setFontBold(true);
    header.setBorderStyle(QXlsx::Format::BorderThin);
    header.setPatternBackgroundColor(QColor(230, 230, 230));
    header.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
    header.setVerticalAlignment(QXlsx::Format::AlignVCenter);

    for (int t = 0; t < tablesData.size(); ++t) {
        const auto& table = tablesData[t];
        QString name = table.sheetName.isEmpty() ? QString("Sheet%1").arg(t + 1) : table.sheetName;

        if (t == 0) {
            xlsx.renameSheet("Sheet1", name);
        }
        else {
            xlsx.addSheet(name);
        }

        QVector<int> colWidth(table.horizontalHeaders.size() + 1, 10);

        auto calcWidth = [](const QString& text) -> int {
            int w = 0;
            for (auto ch : text) {
                w += (ch.unicode() >= 0x4E00) ? 2 : 1;
            }
            return w + 2;
            };
        for (int c = 0; c < table.horizontalHeaders.size(); ++c) {
            QString headerText = table.horizontalHeaders[c];
            xlsx.write(1, c + 2, headerText, header);
            colWidth[c + 1] = qBound(colWidth[c + 1], calcWidth(headerText), 50);
        }
        for (int r = 0; r < table.cells.size(); ++r) {
            if (r < table.verticalHeaders.size()) {
                QString vHeader = table.verticalHeaders[r];
                xlsx.write(r + 2, 1, vHeader, header);
                colWidth[0] = qBound(colWidth[0], calcWidth(vHeader), 50);
            }
            for (int c = 0; c < table.cells[r].size(); ++c) {
                const QString& text = table.cells[r][c];
                xlsx.write(r + 2, c + 2, text);

                if (c + 1 < colWidth.size()) {
                    colWidth[c + 1] = qBound(colWidth[c + 1], calcWidth(text), 50);
                }
            }
        }
        for (int i = 0; i < colWidth.size(); ++i) {
            xlsx.setColumnWidth(i + 1, colWidth[i]);
        }
    }

    bool ok = xlsx.saveAs(filePath);
    Q_EMIT excelExportFinished(ok, ok ? "" : "保存失败，请检查文件是否被占用。");
}

//放弃了直接在程序黎明直接写所有需要的逻辑，转而嵌个AI或者脚本读取引擎。
void BackendWorker::autoSchedule(const QHash<QString, QStringList>& available, int targetPerCell, bool enablePrioritize, bool isMultipate, int mode) {
    _abort.store(false);
    QHash<QString, QList<QUuid>> schedule;
    QMap<QUuid, int> userDutyCount;
    QSet<QUuid> scheduledThisWeek;

    QList<QString> cellKeys = available.keys();
    //优先处理空闲人员少的格子
    std::sort(cellKeys.begin(), cellKeys.end(), [&](const QString& a, const QString& b) {
        return available.value(a).size() < available.value(b).size();
        });
    std::random_device rd;
    std::mt19937 g(rd());
    for (const QString& key : cellKeys) {
        if (_abort.load()) {
            Q_EMIT autoScheduleFinished(QHash<QString, QList<QUuid>>());
            return;
        }
        QStringList availableStrs = available.value(key);
        QList<QUuid> selectedForCell;

        if (enablePrioritize) {
            //次数少的优先
            std::sort(availableStrs.begin(), availableStrs.end(), [&](const QString& aStr, const QString& bStr) {
                QUuid a(aStr), b(bStr);
                return userDutyCount.value(a, 0) < userDutyCount.value(b, 0);
                });
        }
        else {
            //同级优先情况下随机
            std::shuffle(availableStrs.begin(), availableStrs.end(), g);
        }

        for (const QString& uidStr : availableStrs) {
            if (selectedForCell.size() >= targetPerCell) break;
            QUuid uid(uidStr);
            if (mode == 0 && !isMultipate) {
                if (scheduledThisWeek.contains(uid)) {
					continue;
                }
            }
            selectedForCell.append(uid);
            userDutyCount[uid]++;
            if (mode == 0 && !isMultipate) {
                scheduledThisWeek.insert(uid);
            }
        }
        schedule[key] = selectedForCell;
    }
    Q_EMIT autoScheduleFinished(schedule);
}

void BackendWorker::scriptSchedule(const QString& scriptPath, int targetPerCell, const QHash<QString, QStringList>& available, const QList<MemberInfoModel::MemberData>& allMembersData)
{
    _abort.store(false);
    QString debugMsg;
    qDebug() << "开始执行:" << scriptPath;
    QHash<QString, QList<QUuid>> schedule = ScriptEngine::instance()->runScheduleScript(scriptPath, targetPerCell, available, allMembersData, debugMsg);
    if (_abort.load()) {
        Q_EMIT scriptScheduleFinished(QHash<QString, QList<QUuid>>(), "排班已被用户中止");
        return;
    }
    Q_EMIT scriptScheduleFinished(schedule, debugMsg);
}

void BackendWorker::abortCurrentTask() {
    _abort.store(true);
}

void BackendWorker::searchMember(const QList<MemberInfoModel::MemberData>& allMembersData) {
    _abort.store(false);
    QStringList names;
    QList<QVariantMap> dataList;
    names.reserve(allMembersData.size());
    dataList.reserve(allMembersData.size());

    for (int i = 0; i < allMembersData.size(); ++i) {
        if (_abort.load()) return;

        QString name = allMembersData[i].properties.value("姓名").toString();
        if (name.isEmpty()) continue;
        names.append(name);
        QVariantMap data;
        data.insert("row", i);
        data.insert("name", name);
        dataList.append(data);
    }

    Q_EMIT searchMemberFinished(names, dataList);
}

#ifdef ENABLE_AI_SUPPORT
void BackendWorker::AISchedule(const QString& userPrompt, const QString& compressedData,
    const QMap<QString, QUuid>& nameMap,
    const QMap<QString, QString>& taskSlotMap, bool enableThinking) {
    if (_abort.load()) {
        Q_EMIT aiScheduleFinished(false, {}, "用户中止");
        return;
    }

    if (nameMap.isEmpty()) {
        Q_EMIT aiScheduleFinished(false, {}, "没有可排班的成员");
        return;
    }

    _aiService = AIServiceInterface::instance();
    if (!_aiService) {
        Q_EMIT aiScheduleFinished(false, {}, "AI 服务未初始化");
        return;
    }

    QString systemPrompt = "你是专业排班AI。按要求分配成员。\n"
        "规则：\n"
        "1. 每个任务槽默认分配2人（不足则分配1人）\n"
        "2. 尽量不重复选择同一成员\n"
        "3. 只输出一个纯JSON对象，不要包含任何其他文字、解释或Markdown标记\n"
        "4. JSON必须在一行内，不得换行\n"
        "输出示例：{\"周一_08:00-10:00\":[\"张三\",\"李四\"],\"周一_10:00-12:00\":[\"王五\",\"赵六\"]}\n"
        "注意：槽位描述格式为 星期/日期_时间段，如 周一_08:00-10:00 或 2025-03-15_14:00-16:00";

    QString finalPrompt = systemPrompt + "\n" + compressedData +
        "\n【特殊要求】:" + userPrompt + "\n输出JSON:";

    qDebug() << "提示词长度:" << finalPrompt.size() << "字符";

    if (finalPrompt.toUtf8().size() > 100000) {
        Q_EMIT aiScheduleFinished(false, {}, "提示词超过10万字节，请减少成员或时间段");
        return;
    }

    QString aiResponse = _aiService->generateResponseStreaming(finalPrompt, enableThinking);
    /*qDebug() << "=== AI 原始响应 ===" << aiResponse << "========";*/

    if (_abort.load()) {
        Q_EMIT aiScheduleFinished(false, {}, "用户中止");
        return;
    }

    if (aiResponse.startsWith("API Error:") || aiResponse.startsWith("HTTP Error")) {
        qDebug() << "DeepSeek API 错误:" << aiResponse;
        Q_EMIT aiScheduleFinished(false, {}, "AI接口错误：" + aiResponse);
        return;
    }

    aiResponse = aiResponse.trimmed();

    // 去除可能的 Markdown 代码块
    if (aiResponse.startsWith("```json")) {
        aiResponse = aiResponse.mid(7);
        if (aiResponse.endsWith("```")) aiResponse.chop(3);
    }
    else if (aiResponse.startsWith("```")) {
        aiResponse = aiResponse.mid(3);
        if (aiResponse.endsWith("```")) aiResponse.chop(3);
    }

    int firstBrace = aiResponse.lastIndexOf('{');
    int lastBrace = aiResponse.lastIndexOf('}');

    if (firstBrace != -1 && lastBrace != -1 && lastBrace > firstBrace) {
        aiResponse = aiResponse.mid(firstBrace, lastBrace - firstBrace + 1);

        //如果花括号不匹配，尝试从第一个 '{' 重新提取
        int braceCount = 0;
        for (int i = 0; i < aiResponse.size(); ++i) {
            if (aiResponse[i] == '{') braceCount++;
            else if (aiResponse[i] == '}') braceCount--;
        }
        if (braceCount != 0) {
            firstBrace = aiResponse.indexOf('{');
            lastBrace = aiResponse.lastIndexOf('}');
            if (firstBrace != -1 && lastBrace > firstBrace)
                aiResponse = aiResponse.mid(firstBrace, lastBrace - firstBrace + 1);
        }
    }
    else {
        qDebug() << "返回非法格式：" << aiResponse;
        Q_EMIT aiScheduleFinished(false, {}, "AI返回格式错误，未找到完整JSON");
        return;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(aiResponse.toUtf8(), &error);
    QHash<QString, QList<QUuid>> finalResults;

    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            QString slotDesc = it.key(); 
            QString realKey = taskSlotMap.value(slotDesc);
            if (realKey.isEmpty()) continue;

            QJsonArray arr = it.value().toArray();
            QList<QUuid> uuids;
            for (int i = 0; i < arr.size(); ++i) {
                QString name = arr[i].toString();
                if (nameMap.contains(name))
                    uuids.append(nameMap.value(name));
            }
            finalResults.insert(realKey, uuids);
        }
        Q_EMIT aiScheduleFinished(true, finalResults, "");
    }
    else {
        qDebug() << "JSON 解析失败:" << error.errorString();
        Q_EMIT aiScheduleFinished(false, {}, "AI返回JSON解析失败：" + error.errorString());
    }
}
#endif

void BackendWorker::resetAbort() { 
    _abort.store(false); 
}
