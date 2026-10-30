#include "feishuscheduleexporter.h"

#include "xlsxcell.h"
#include "xlsxcellformula.h"
#include "xlsxcellreference.h"
#include "xlsxdocument.h"
#include "xlsxworksheet.h"
#include "xlsxzipreader_p.h"
#include "xlsxzipwriter_p.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTime>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <algorithm>

namespace {
struct Interval {
    QTime start;
    QTime end;
    QString text;
};

struct PreparedExport {
    QByteArray package;
    QString sheetPath;
    FeishuTemplateInfo info;
    QMap<int, QMap<int, QString>> combinations;
};

QStringList visibleWorksheets(QXlsx::Document& workbook) {
    QStringList result;
    for (const auto& name : workbook.sheetNames()) {
        auto* sheet = workbook.sheet(name);
        if (sheet && sheet->sheetType() == QXlsx::AbstractSheet::ST_WorkSheet && sheet->isVisible())
            result.append(name);
    }
    return result;
}

bool readPackage(const QString& path, QByteArray& bytes, QString& error) {
    if (QFileInfo(path).suffix().compare("xlsx", Qt::CaseInsensitive) != 0) {
        error = "请选择从飞书下载的 .xlsx 排班模板。";
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = "无法读取排班模板：" + file.errorString();
        return false;
    }
    bytes = file.readAll();
    if (file.error() != QFileDevice::NoError || bytes.isEmpty()) {
        error = "排班模板为空或读取失败。";
        return false;
    }
    return true;
}

QString normalizedHeading(QString text) {
    QString heading;
    for (const auto character : text) {
        if (!character.isSpace() && character != '*' && character != QChar(0xff0a)) heading += character;
    }
    return heading.toCaseFolded();
}

QDate headerDate(const std::shared_ptr<QXlsx::Cell>& cell, const QDate& month) {
    if (!cell) return {};
    const QVariant value = cell->readValue();
    if (cell->isDateTime()) {
        const auto date = cell->dateTime().toDate();
        if (date.isValid()) return date;
        const auto dateTime = cell->dateTime().toDateTime();
        if (dateTime.isValid()) return dateTime.date();
    }
    if (value.metaType().id() == QMetaType::QDate) return value.toDate();
    if (value.metaType().id() == QMetaType::QDateTime) return value.toDateTime().date();
    const auto text = value.toString().trimmed();
    static const QRegularExpression fullDate(
        "^(\\d{4})[-/.年](\\d{1,2})[-/.月](\\d{1,2})日?(?=\\s|[（(]|$)");
    const auto full = fullDate.match(text);
    if (full.hasMatch()) return QDate(full.captured(1).toInt(), full.captured(2).toInt(), full.captured(3).toInt());
    if (month.isValid()) {
        static const QRegularExpression dayNumber("^(\\d{1,2})日?(?=\\s|[（(]|$)");
        const auto day = dayNumber.match(text);
        if (day.hasMatch()) return QDate(month.year(), month.month(), day.captured(1).toInt());
    }
    return {};
}

bool parseInterval(const QString& range, Interval& result) {
    const auto parts = range.split('-');
    if (parts.size() != 2) return false;
    result.start = QTime::fromString(parts[0].trimmed(), "HH:mm");
    result.end = QTime::fromString(parts[1].trimmed(), "HH:mm");
    if (!result.start.isValid() || !result.end.isValid() || result.start >= result.end) return false;
    result.text = result.start.toString("HH:mm") + "-" + result.end.toString("HH:mm");
    return true;
}

bool unavailableGray(const QXlsx::Format& format) {
    if (format.fillPattern() == QXlsx::Format::PatternNone) return false;
    const auto isGray = [](const QColor& color) {
        if (!color.isValid() || color.alpha() == 0) return false;
        const int darkest = qMin(color.red(), qMin(color.green(), color.blue()));
        const int lightest = qMax(color.red(), qMax(color.green(), color.blue()));
        return lightest - darkest <= 12 && darkest >= 32 && lightest <= 245;
    };
    return isGray(format.patternForegroundColor()) || isGray(format.patternBackgroundColor());
}

QMap<int, QString> rawIdentifiers(const QByteArray& xml, int column, QXlsx::Worksheet* worksheet,
                                 QString& error) {
    QMap<int, QString> identifiers;
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != "c") continue;
        const QXlsx::CellReference reference(reader.attributes().value("r").toString());
        if (!reference.isValid() || reference.column() != column) {
            reader.skipCurrentElement();
            continue;
        }
        const bool sharedString = reader.attributes().value("t") == "s";
        QString identifier;
        bool formula = false;
        while (!reader.atEnd()) {
            reader.readNext();
            if (reader.isEndElement() && reader.name() == "c") break;
            if (!reader.isStartElement()) continue;
            if (reader.name() == "f") formula = true;
            if (reader.name() == "v" || reader.name() == "t")
                identifier += reader.readElementText();
        }
        if (sharedString) {
            const auto cell = worksheet->cellAt(reference.row(), column);
            const auto value = cell ? cell->readValue() : QVariant{};
            // The shared string itself is already text; never convert a numeric UID through double.
            identifier = value.metaType().id() == QMetaType::QString ? value.toString() : QString{};
        }
        if (!formula && !identifier.trimmed().isEmpty()) identifiers[reference.row()] = identifier.trimmed();
    }
    if (reader.hasError()) error = "模板人员标识所在工作表 XML 无效。";
    return identifiers;
}

bool prepare(const FeishuTemplateSettings& settings, const QList<FeishuDutySlot>& duties,
             PreparedExport& prepared, QString& error) {
    error.clear();
    if (settings.headerRow < 1 || settings.headerRow > 1048576) {
        error = "模板表头行号无效。";
        return false;
    }
    if (!readPackage(settings.filePath, prepared.package, error)) return false;
    QBuffer input(&prepared.package);
    input.open(QIODevice::ReadOnly);
    QXlsx::Document workbook(&input);
    if (!workbook.load()) {
        error = "无法解析 Excel 排班模板，请重新从飞书下载 .xlsx 文件。";
        return false;
    }
    prepared.info.sheetNames = visibleWorksheets(workbook);
    prepared.info.sheetName = settings.sheetName;
    if (prepared.info.sheetName.isEmpty() && prepared.info.sheetNames.size() == 1)
        prepared.info.sheetName = prepared.info.sheetNames.first();
    if (!prepared.info.sheetNames.contains(prepared.info.sheetName)
        || !workbook.selectSheet(prepared.info.sheetName)) {
        error = "请选择模板中可见的排班工作表。";
        return false;
    }
    auto* worksheet = workbook.currentWorksheet();
    prepared.sheetPath = worksheet->filePath();
    int uidColumn = 0;
    const auto dimension = worksheet->dimension();
    for (int column = 1; column <= dimension.lastColumn(); ++column) {
        const auto cell = worksheet->cellAt(settings.headerRow, column);
        if (!cell) continue;
        const auto heading = normalizedHeading(cell->readValue().toString());
        if (heading == "uid") {
            if (uidColumn != 0) {
                error = "模板中包含多个 UID 列，无法确定飞书人员标识。";
                return false;
            }
            uidColumn = column;
            continue;
        }
        if (heading == "姓名" || heading == "name") {
            if (prepared.info.nameColumn != 0) {
                error = "模板表头包含多个姓名 / Name 列，无法确定人员行。";
                return false;
            }
            prepared.info.nameColumn = column;
            continue;
        }
        const auto date = headerDate(cell, settings.month);
        if (date.isValid()) {
            if (prepared.info.dateColumns.contains(date)) {
                error = "模板中同一日期出现多列：" + date.toString("yyyy-MM-dd");
                return false;
            }
            prepared.info.dateColumns.insert(date, column);
        }
    }
    if (prepared.info.nameColumn == 0) {
        error = "指定表头行未找到姓名 / Name 列，请检查模板和表头行号。";
        return false;
    }
    if (prepared.info.dateColumns.isEmpty()) {
        error = "指定表头行没有可识别的日期；仅含日号的模板还需指定正确月份。";
        return false;
    }
    if (uidColumn == 0) {
        error = "模板缺少飞书 UID 唯一标识列，请重新下载原始排班模板，不能仅创建姓名和日期表头。";
        return false;
    }
    QBuffer original(&prepared.package);
    original.open(QIODevice::ReadOnly);
    QXlsx::ZipReader package(&original);
    const auto paths = package.filePaths();
    if (prepared.sheetPath.startsWith('/')) prepared.sheetPath.remove(0, 1);
    if (!paths.contains(prepared.sheetPath) && paths.contains("xl/" + prepared.sheetPath))
        prepared.sheetPath.prepend("xl/");
    if (!paths.contains(prepared.sheetPath)) {
        error = "无法定位模板原始排班工作表。";
        return false;
    }
    const auto uids = rawIdentifiers(package.fileData(prepared.sheetPath), uidColumn, worksheet, error);
    if (!error.isEmpty()) return false;
    QMap<QString, QList<int>> memberRows;
    int lastRow = 0, lastColumn = 0;
    const auto cells = worksheet->getFullCells(&lastRow, &lastColumn);
    for (const auto& cell : cells) {
        if (cell.row <= settings.headerRow || cell.col != prepared.info.nameColumn || !cell.cell) continue;
        const auto name = cell.cell->readValue().toString().trimmed();
        if (!name.isEmpty()) memberRows[name].append(cell.row);
    }
    if (duties.isEmpty()) {
        error = "当前视图没有已排班的成员，无法生成飞书排班文件。";
        return false;
    }
    if (memberRows.isEmpty()) {
        error = "模板只有表头，没有人员名单。请在飞书目标考勤组的“排班表 → 导入/导出 → 当前排班表”"
                "下载包含姓名和 UID 的文件，放入 excelTemplate 文件夹后在设置中选择；不能仅凭姓名生成飞书 UID。";
        return false;
    }
    QMap<QString, QUuid> nameIds;
    QMap<QUuid, QString> idNames;
    QMap<QString, QString> uidNames;
    QMap<int, QMap<QDate, QList<Interval>>> dailyIntervals;
    for (const auto& duty : duties) {
        if (settings.templateDatesOnly && duty.date.isValid() && !prepared.info.dateColumns.contains(duty.date)) {
            ++prepared.info.skippedDateSlots;
            continue;
        }
        const auto name = duty.name.trimmed();
        if (duty.memberId.isNull() || name.isEmpty()) {
            error = "已排班记录存在缺失的成员标识或空姓名，请先修正项目成员。";
            return false;
        }
        if ((nameIds.contains(name) && nameIds[name] != duty.memberId)
            || (idNames.contains(duty.memberId) && idNames[duty.memberId] != name)) {
            error = "程序中的已排班成员姓名重复或同一成员姓名不一致，无法仅按姓名匹配：" + name;
            return false;
        }
        nameIds.insert(name, duty.memberId);
        idNames.insert(duty.memberId, name);
        if (memberRows.value(name).size() != 1) {
            error = memberRows.value(name).isEmpty()
                ? "模板中找不到已排班成员，请使用包含该成员的原始飞书模板：" + name
                : "模板中已排班成员存在重名，无法仅按姓名匹配：" + name;
            return false;
        }
        Interval interval;
        if (!parseInterval(duty.timeRange, interval)) {
            error = "已排班记录的时间段无效（暂不支持跨夜班次）：" + duty.timeRange;
            return false;
        }
        QList<QDate> dates;
        if (duty.date.isValid()) {
            if (!prepared.info.dateColumns.contains(duty.date)) {
                error = "模板日期未覆盖已排班记录：" + duty.date.toString("yyyy-MM-dd");
                return false;
            }
            dates.append(duty.date);
        }
        else {
            if (duty.dayOfWeek < 1 || duty.dayOfWeek > 7) {
                error = "周期排班记录缺少有效的星期（周一至周日）。";
                return false;
            }
            for (auto it = prepared.info.dateColumns.cbegin(); it != prepared.info.dateColumns.cend(); ++it) {
                if (it.key().dayOfWeek() == duty.dayOfWeek) dates.append(it.key());
            }
            if (dates.isEmpty()) {
                error = "模板日期未覆盖已排班的星期，请选择覆盖该星期的模板。";
                return false;
            }
        }
        const auto row = memberRows[name].first();
        const auto uid = uids.value(row);
        if (uid.isEmpty()) {
            error = "模板中已排班成员的 UID 缺失或不是固定值，请重新下载原始模板：" + name;
            return false;
        }
        if (uidNames.contains(uid) && uidNames[uid] != name) {
            error = "模板中同一 UID 对应多个已排班姓名，请重新下载原始模板。";
            return false;
        }
        uidNames.insert(uid, name);
        for (const auto& date : dates) dailyIntervals[row][date].append(interval);
    }
    if (dailyIntervals.isEmpty()) {
        error = "模板日期范围内没有已排班记录，未生成输出文件。";
        return false;
    }
    QSet<QString> combinations;
    const auto mergedCells = worksheet->mergedCells();
    for (auto row = dailyIntervals.begin(); row != dailyIntervals.end(); ++row) {
        for (auto day = row.value().begin(); day != row.value().end(); ++day) {
            auto& intervals = day.value();
            std::sort(intervals.begin(), intervals.end(), [](const Interval& left, const Interval& right) {
                return left.start < right.start;
            });
            QStringList parts;
            for (int index = 0; index < intervals.size(); ++index) {
                if (index > 0 && intervals[index].start < intervals[index - 1].end) {
                    error = "同一成员同一天存在重叠或重复时间段：" + day.key().toString("yyyy-MM-dd");
                    return false;
                }
                parts.append(intervals[index].text);
            }
            const int column = prepared.info.dateColumns[day.key()];
            for (const auto& range : mergedCells) {
                if (row.key() >= range.firstRow() && row.key() <= range.lastRow()
                    && column >= range.firstColumn() && column <= range.lastColumn()
                    && (range.rowCount() > 1 || range.columnCount() > 1)) {
                    error = "目标排班单元格包含合并区域，请使用飞书下载的原始排班模板。";
                    return false;
                }
            }
            const auto cell = worksheet->cellAt(row.key(), column);
            if (cell && unavailableGray(cell->format())) {
                error = "已排班记录对应模板灰色不可编辑单元格，该成员当天不在此考勤组："
                    + day.key().toString("yyyy-MM-dd");
                return false;
            }
            if (cell && cell->hasFormula() && cell->formula().formulaType() != QXlsx::CellFormula::NormalType) {
                error = "目标排班单元格包含数组或共享公式，无法安全替换。";
                return false;
            }
            const auto combination = parts.join(" + ");
            prepared.combinations[row.key()][column] = combination;
            combinations.insert(combination);
            ++prepared.info.cellCount;
        }
    }
    prepared.info.shiftCombinations = combinations.values();
    prepared.info.shiftCombinations.sort();
    return true;
}

void copyElement(QXmlStreamReader& reader, QXmlStreamWriter& writer) {
    writer.writeCurrentToken(reader);
    int depth = 1;
    while (depth > 0 && !reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) ++depth;
        if (reader.isEndElement()) --depth;
        writer.writeCurrentToken(reader);
    }
}

void writeCell(QXmlStreamWriter& writer, const QString& xmlNamespace, int row, int column,
               const QString& text, QXmlStreamAttributes attributes = {}) {
    writer.writeStartElement(xmlNamespace, "c");
    for (const auto& attribute : attributes) {
        if (attribute.name() != "t" && attribute.name() != "r") writer.writeAttribute(attribute);
    }
    writer.writeAttribute("r", QXlsx::CellReference(row, column).toString());
    writer.writeAttribute("t", "inlineStr");
    writer.writeStartElement(xmlNamespace, "is");
    writer.writeStartElement(xmlNamespace, "t");
    writer.writeAttribute("http://www.w3.org/XML/1998/namespace", "space", "preserve");
    writer.writeCharacters(text);
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndElement();
}

bool patchWorksheet(const QByteArray& source, const QMap<int, QMap<int, QString>>& changes,
                    QByteArray& output, QString& error) {
    QXmlStreamReader reader(source);
    QXmlStreamWriter writer(&output);
    QSet<int> completedRows;
    int previousRow = 0;
    QString worksheetNamespace;
    bool inSheetData = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == "worksheet")
            worksheetNamespace = reader.namespaceUri().toString();
        if (reader.namespaceUri() == worksheetNamespace && reader.name() == "sheetData")
            inSheetData = reader.isStartElement();
        if (!inSheetData || !reader.isStartElement() || reader.name() != "row"
            || reader.namespaceUri() != worksheetNamespace) {
            writer.writeCurrentToken(reader);
            continue;
        }
        const QString xmlNamespace = reader.namespaceUri().toString();
        bool hasNumber = false;
        const int explicitRow = reader.attributes().value("r").toInt(&hasNumber);
        const int row = hasNumber ? explicitRow : previousRow + 1;
        previousRow = row;
        if (!changes.contains(row)) {
            copyElement(reader, writer);
            continue;
        }
        if (completedRows.contains(row)) {
            error = "模板包含重复的行号，无法安全写入。";
            return false;
        }
        completedRows.insert(row);
        auto remaining = changes[row];
        writer.writeCurrentToken(reader);
        int previousColumn = 0;
        while (!reader.atEnd()) {
            reader.readNext();
            if (reader.isEndElement()) {
                for (auto it = remaining.cbegin(); it != remaining.cend(); ++it)
                    writeCell(writer, xmlNamespace, row, it.key(), it.value());
                writer.writeCurrentToken(reader);
                break;
            }
            if (!reader.isStartElement()) {
                writer.writeCurrentToken(reader);
                continue;
            }
            if (reader.name() != "c" || reader.namespaceUri() != xmlNamespace) {
                copyElement(reader, writer);
                continue;
            }
            const QString reference = reader.attributes().value("r").toString();
            const QXlsx::CellReference cell(reference);
            const int column = reference.isEmpty() ? previousColumn + 1 : cell.column();
            if (column <= previousColumn || (!reference.isEmpty() && (!cell.isValid() || cell.row() != row))) {
                error = "模板单元格坐标或顺序无效，无法安全写入。";
                return false;
            }
            previousColumn = column;
            while (!remaining.isEmpty() && remaining.firstKey() < column) {
                const int pendingColumn = remaining.firstKey();
                writeCell(writer, xmlNamespace, row, pendingColumn, remaining.take(pendingColumn));
            }
            if (remaining.contains(column)) {
                writeCell(writer, xmlNamespace, row, column, remaining.take(column), reader.attributes());
                reader.skipCurrentElement();
            }
            else copyElement(reader, writer);
        }
    }
    if (reader.hasError() || writer.hasError() || completedRows.size() != changes.size()) {
        error = "模板工作表 XML 无效或目标人员行已缺失，未写入输出文件。";
        return false;
    }
    return true;
}

bool samePath(const QString& source, const QString& destination) {
    const QFileInfo input(source), output(destination);
    const auto sourcePath = input.canonicalFilePath();
    const auto outputPath = output.exists() ? output.canonicalFilePath() : QDir::cleanPath(output.absoluteFilePath());
    return !sourcePath.isEmpty() && sourcePath.compare(outputPath, Qt::CaseInsensitive) == 0;
}
}

QStringList FeishuScheduleExporter::sheetNames(const QString& filePath, QString& error) {
    error.clear();
    QByteArray bytes;
    if (!readPackage(filePath, bytes, error)) return {};
    QBuffer input(&bytes);
    input.open(QIODevice::ReadOnly);
    QXlsx::Document workbook(&input);
    if (!workbook.load()) {
        error = "无法解析 Excel 排班模板。";
        return {};
    }
    const auto names = visibleWorksheets(workbook);
    if (names.isEmpty()) error = "模板没有可见的排班工作表。";
    return names;
}

bool FeishuScheduleExporter::inspect(const FeishuTemplateSettings& settings,
                                   const QList<FeishuDutySlot>& duties,
                                   FeishuTemplateInfo& info, QString& error) {
    info = {};
    PreparedExport prepared;
    if (!prepare(settings, duties, prepared, error)) return false;
    info = prepared.info;
    return true;
}

bool FeishuScheduleExporter::exportFile(const FeishuExportRequest& request, QString& message) {
    message.clear();
    if (request.outputPath.trimmed().isEmpty()
        || QFileInfo(request.outputPath).suffix().compare("xlsx", Qt::CaseInsensitive) != 0) {
        message = "请指定新的 .xlsx 输出文件。";
        return false;
    }
    if (samePath(request.settings.filePath, request.outputPath)) {
        message = "输出文件不能覆盖原始飞书模板，请使用新的文件名。";
        return false;
    }
    PreparedExport prepared;
    if (!prepare(request.settings, request.duties, prepared, message)) return false;
    static const QRegularExpression invalidXml("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F]");
    for (auto row = prepared.combinations.begin(); row != prepared.combinations.end(); ++row) {
        for (auto cell = row.value().begin(); cell != row.value().end(); ++cell) {
            const auto suppliedName = request.shiftMapping.value(cell.value());
            const auto label = suppliedName.trimmed();
            if (label.isEmpty() || label.size() > 64 || suppliedName.contains('\n')
                || suppliedName.contains('\r') || suppliedName.contains(invalidXml)) {
                message = "请填写飞书已创建的单个班次名称，最多64字，不能多行。对应时间段组合：" + cell.value();
                return false;
            }
            cell.value() = label;
        }
    }
    QBuffer original(&prepared.package);
    original.open(QIODevice::ReadOnly);
    QXlsx::ZipReader package(&original);
    const auto paths = package.filePaths();
    if (prepared.sheetPath.startsWith('/')) prepared.sheetPath.remove(0, 1);
    if (!paths.contains(prepared.sheetPath) && paths.contains("xl/" + prepared.sheetPath))
        prepared.sheetPath.prepend("xl/");
    if (!paths.contains(prepared.sheetPath)) {
        message = "无法定位模板原始工作表，未写入输出文件。";
        return false;
    }
    QByteArray changedSheet;
    if (!patchWorksheet(package.fileData(prepared.sheetPath), prepared.combinations, changedSheet, message)) return false;
    // The bundled ZIP writer closes its device, which QSaveFile explicitly forbids.
    // Finish the ZIP in a buffer, then atomically commit those complete bytes.
    QByteArray completedPackage;
    QBuffer packageOutput(&completedPackage);
    packageOutput.open(QIODevice::WriteOnly);
    bool packageWritten = false;
    {
        QXlsx::ZipWriter writer(&packageOutput);
        for (const auto& path : paths)
            writer.addFile(path, path == prepared.sheetPath ? changedSheet : package.fileData(path));
        writer.close();
        packageWritten = !writer.error();
    }
    if (!packageWritten) {
        message = "生成 Excel 文件失败，未写入输出文件。";
        return false;
    }
    QSaveFile output(request.outputPath);
    if (!output.open(QIODevice::WriteOnly)) {
        message = "无法创建输出文件：" + output.errorString();
        return false;
    }
    if (output.write(completedPackage) != completedPackage.size() || !output.commit()) {
        output.cancelWriting();
        message = "保存失败，原输出文件未替换；请检查磁盘空间或文件占用。";
        return false;
    }
    message = QString("已填入 %1 个人员日期单元格；未排班内容和模板人员标识保持不变。")
        .arg(prepared.info.cellCount);
    if (prepared.info.skippedDateSlots > 0)
        message += QString("已按选择排除模板日期之外的 %1 条排班记录。").arg(prepared.info.skippedDateSlots);
    return true;
}
