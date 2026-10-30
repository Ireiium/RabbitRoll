#pragma once

#include <QDate>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QUuid>

struct FeishuDutySlot {
    QUuid memberId;
    QString name;
    QString timeRange;
    QDate date;
    int dayOfWeek{0};
};

struct FeishuTemplateSettings {
    QString filePath;
    QString sheetName;
    int headerRow{4};
    QDate month;
    bool templateDatesOnly{false};
};

struct FeishuTemplateInfo {
    QStringList sheetNames;
    QString sheetName;
    int nameColumn{0};
    QMap<QDate, int> dateColumns;
    QStringList shiftCombinations;
    int cellCount{0};
    int skippedDateSlots{0};
};

struct FeishuExportRequest {
    FeishuTemplateSettings settings;
    QString outputPath;
    QList<FeishuDutySlot> duties;
    QMap<QString, QString> shiftMapping;
};

namespace FeishuScheduleExporter {
QStringList sheetNames(const QString& filePath, QString& error);
bool inspect(const FeishuTemplateSettings& settings, const QList<FeishuDutySlot>& duties,
             FeishuTemplateInfo& info, QString& error);
bool exportFile(const FeishuExportRequest& request, QString& message);
}
