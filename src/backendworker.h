#pragma once
#include <QObject>
#include <QJsonObject>
#include <QDate>
#include <QStringList>
#include "memberinfomodel.h"
#include <atomic>

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
    void autoSchedule(const QHash<QString, QStringList>& available, int targetPerCell, bool enablePrioritize, bool isMultipate, int mode);
    void scriptSchedule(const QString& scriptPath, int targetPerCell, const QHash<QString, QStringList>& available, const QList<MemberInfoModel::MemberData>& allMembersData);
    void searchMember(const QList<MemberInfoModel::MemberData>& allMembersData);

Q_SIGNALS:
    void batchMembersFinished(const QStringList& vMembers);
    void batchMembersWithFormatFinished(const QList<QVariantMap>& membersData, const QStringList& format);
    void scheduleImportFinished(const QJsonObject& schedule);
    void excelExportFinished(bool success, const QString& debugMsg);
    void autoScheduleFinished(const QHash<QString, QList<QUuid>>& schedule);
    void scriptScheduleFinished(const QHash<QString, QList<QUuid>>& schedule, const QString& debugMsg);
    void searchMemberFinished(const QStringList& names, const QList<QVariantMap>& dataList);
#ifdef ENABLE_AI_SUPPORT
    void aiScheduleFinished(bool success, const QHash<QString, QList<QUuid>>& schedule, const QString& debugMsg);
#endif

public:
    static BackendWorker* instance();
#ifdef ENABLE_AI_SUPPORT
    void runAISchedule(const QString& prompt, const QHash<QString,QStringList>& candidates,
        const QList<MemberInfoModel::MemberData>& members, bool thinking);
    static QHash<QString,QList<QUuid>> calculateAISchedule(const QString& prompt,
        const QHash<QString,QStringList>& candidates, const QList<MemberInfoModel::MemberData>& members,
        int targetPerCell, bool thinking, const std::atomic<bool>* cancelled, QString& error);
#endif
    static bool writeExcelFile(const QString& filePath, const QList<ExportTableData>& tablesData, QString& error);
    // Shared, model-free algorithm used by GUI work and MCP background jobs.
    static QHash<QString, QList<QUuid>> calculateAutoSchedule(
        const QHash<QString, QStringList>& available, int targetPerCell,
        bool enablePrioritize, bool isMultipate, int mode,
        const std::atomic<bool>* cancelled = nullptr);
    void abortCurrentTask();
    void resetAbort();

private:
    explicit BackendWorker(QObject* parent = nullptr);
    std::atomic<bool> _abort{ false };

};

Q_DECLARE_METATYPE(ExportTableData)
Q_DECLARE_METATYPE(QList<ExportTableData>)

