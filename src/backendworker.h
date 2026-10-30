#pragma once
#include <QObject>
#include <QJsonObject>
#include <QDate>
#include <QStringList>
#include "memberinfomodel.h"
#include "feishuscheduleexporter.h"
#include <atomic>

class AIServiceInterface;

struct ExportTableData {
    QString sheetName;
    QStringList horizontalHeaders;
    QStringList verticalHeaders;
    QList<QStringList> cells;//[row][col] 
};

class BackendWorker : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void batchMembers(const QStringList& members);
    void batchMembersWithFormat(const QStringList& lines, const QStringList& format);
    void scheduleImport(const QJsonObject& json, QDate startDate);
    void exportExcel(const QString& filePath, const QList<ExportTableData>& tablesData);
    void exportFeishuExcel(const FeishuExportRequest& request);
    void autoSchedule(const QHash<QString, QStringList>& available, int targetPerCell, bool enablePrioritize, bool isMultipate, int mode);
    void scriptSchedule(const QString& scriptPath, int targetPerCell, const QHash<QString, QStringList>& available, const QList<MemberInfoModel::MemberData>& allMembersData);
    void searchMember(const QList<MemberInfoModel::MemberData>& allMembersData);
#ifdef ENABLE_AI_SUPPORT
    void AISchedule(const QString& userPrompt, const QString& compressedData,
        const QMap<QString, QUuid>& nameMap,
        const QMap<QString, QString>& taskSlotMap, bool enableThinking);
#endif

Q_SIGNALS:
    void batchMembersFinished(const QStringList& vMembers);
    void batchMembersWithFormatFinished(const QList<QVariantMap>& membersData, const QStringList& format);
    void scheduleImportFinished(const QJsonObject& schedule);
    void excelExportFinished(bool success, const QString& debugMsg);
    void feishuExportFinished(bool success, const QString& message);
    void autoScheduleFinished(const QHash<QString, QList<QUuid>>& schedule);
    void scriptScheduleFinished(const QHash<QString, QList<QUuid>>& schedule, const QString& debugMsg);
    void searchMemberFinished(const QStringList& names, const QList<QVariantMap>& dataList);
#ifdef ENABLE_AI_SUPPORT
    void aiScheduleFinished(bool success, const QHash<QString, QList<QUuid>>& schedule, const QString& debugMsg);
#endif

public:
    static BackendWorker* instance();
    void abortCurrentTask();
    void resetAbort();

private:
    explicit BackendWorker(QObject* parent = nullptr);
    std::atomic<bool> _abort{ false };

#ifdef ENABLE_AI_SUPPORT
    AIServiceInterface* _aiService{ nullptr };
#endif
};

Q_DECLARE_METATYPE(ExportTableData)
Q_DECLARE_METATYPE(QList<ExportTableData>)
Q_DECLARE_METATYPE(FeishuExportRequest)

