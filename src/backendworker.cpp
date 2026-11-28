#include "backendworker.h"
#include "rschimport.h"
#include <QJsonObject>
#include "xlsxdocument.h"
#include "scriptengine.h"
#include <random>
#include <algorithm> 
#include <QRegularExpression>


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
    Q_EMIT scheduleImportFinished(RschImport::schedule(json, startDate));
}

void BackendWorker::exportExcel(const QString& filePath, const QList<ExportTableData>& tablesData) {
    QString message;
    const bool ok = writeExcelFile(filePath,tablesData,message);
    Q_EMIT excelExportFinished(ok,message);
}

bool BackendWorker::writeExcelFile(const QString& filePath, const QList<ExportTableData>& tablesData, QString& error) {
    error.clear();
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
            if (!xlsx.renameSheet("Sheet1", name)) xlsx.addSheet(name);
        }
        else {
            xlsx.addSheet(name);
        }

        xlsx.selectSheet(name);
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
    if (!ok) error = "保存失败，请检查磁盘空间、权限和文件是否被占用。";
    return ok;
}

//放弃了直接在程序黎明直接写所有需要的逻辑，转而嵌个AI或者脚本读取引擎。
void BackendWorker::autoSchedule(const QHash<QString, QStringList>& available, int targetPerCell, bool enablePrioritize, bool isMultipate, int mode) {
    _abort.store(false);
    Q_EMIT autoScheduleFinished(calculateAutoSchedule(available, targetPerCell,
        enablePrioritize, isMultipate, mode, &_abort));
}

QHash<QString, QList<QUuid>> BackendWorker::calculateAutoSchedule(
    const QHash<QString, QStringList>& available, int targetPerCell,
    bool enablePrioritize, bool isMultipate, int mode, const std::atomic<bool>* cancelled) {
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
        if (cancelled && cancelled->load()) return {};
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
    return schedule;
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

void BackendWorker::resetAbort() { 
    _abort.store(false); 
}
