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

public Q_SLOTS:
    void handleOpenProject();
    void handleOpenRecentProject(const QString& filePath);
    bool handleSaveProject();
    bool handleSaveAsProject();
    void handleExportScheduleExcel();
    void handleExportFeishuScheduleExcel();
    void handleImportMembersData();
    void handleExportMembersExcel();
    void handleExportMembersData();
    void handleExportSchedule();

Q_SIGNALS:
    void projectLoadedSuccessfully(const QString& filePath);
    void projectSavedSuccessfully(const QString& filePath);
    void requestExportExcel(const QString& filePath, const QList<ExportTableData>& tablesData);
    void requestExportFeishuExcel(const FeishuExportRequest& request);
    void ImportMembersDataSuccessfully();

private:
    QWidget* _parentUI;
};

