#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include "backendworker.h"

class QWidget;

class ProjectActionController : public QObject
{
    Q_OBJECT
public:
    explicit ProjectActionController(QWidget* parentUI = nullptr);
    // Capture display-model values on the GUI thread for both UI and MCP exports.
    static QList<ExportTableData> excelExportSnapshot(bool schedule, bool members);

public Q_SLOTS:
    void handleOpenProject();
    void handleOpenRecentProject(const QString& filePath);
    bool handleSaveProject();
    bool handleSaveAsProject();
    void handleExportScheduleExcel();
    void handleImportMembersData();
    void handleExportMembersExcel();
    void handleExportMembersData();
    void handleExportSchedule();

Q_SIGNALS:
    void projectLoadedSuccessfully(const QString& filePath);
    void projectSavedSuccessfully(const QString& filePath);
    void requestExportExcel(const QString& filePath, const QList<ExportTableData>& tablesData);
    void ImportMembersDataSuccessfully();

private:
    QWidget* _parentUI;
};

