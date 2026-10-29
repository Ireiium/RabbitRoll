#include "projectactioncontroller.h"
#include "projectmanager.h"
#include "projectsetting.h"
#include <QFileDialog>
#include <QProgressDialog>
#include "componentmanager.h"
#include "backendworker.h"
#include <QMessageBox>
#include <QEventLoop>
#include <QWidget>
#include <QProgressBar>

ProjectActionController::ProjectActionController(QWidget* parentUI)
    : QObject(parentUI), _parentUI(parentUI)
{
    connect(this, &ProjectActionController::requestExportExcel, BackendWorker::instance(), &BackendWorker::exportExcel);
}

void ProjectActionController::handleOpenProject() {
    QString filePath = QFileDialog::getOpenFileName(_parentUI, "打开项目", "", "RabbitRoll (*.rroll)");
    if (filePath.isEmpty()) return;

    handleOpenRecentProject(filePath);
}

void ProjectActionController::handleOpenRecentProject(const QString& filePath) {
    if (filePath.isEmpty()) return;

    QProgressDialog progress("正在努力读取中...", QString(), 0, 0, _parentUI);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    QProgressBar* bar = progress.findChild<QProgressBar*>();
    bar->setAlignment(Qt::AlignCenter);

    QEventLoop loop;
    bool success = true;

    QMetaObject::Connection con1 = connect(ProjectManager::instance(), &ProjectManager::projectLoaded, &loop, &QEventLoop::quit);
    QMetaObject::Connection con2 = connect(ProjectManager::instance(), &ProjectManager::operationError, [&](QString msg) {
        QMessageBox::critical(_parentUI, "读取失败", msg);
        success = false;
        loop.quit();
        });

    ProjectManager::instance()->openProject(filePath);
    loop.exec();

    disconnect(con1);
    disconnect(con2);

    if (success) {
        Q_EMIT projectLoadedSuccessfully(filePath);
    }
}

bool ProjectActionController::handleSaveProject() {
    if (ProjectManager::instance()->checkSaveStatus() == ProjectManager::SaveStatus::NeedPath) {
        return handleSaveAsProject();
    }

    QProgressDialog progress("正在安全保存...", QString(), 0, 0, _parentUI);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    QProgressBar* bar = progress.findChild<QProgressBar*>();
    bar->setAlignment(Qt::AlignCenter);

    QEventLoop loop;
    bool isOk = false;

    QMetaObject::Connection con1 = connect(ProjectManager::instance(), &ProjectManager::saveFinished, [&](bool ok) {
        isOk = ok;
        loop.quit();
        });
    QMetaObject::Connection con2 = connect(ProjectManager::instance(), &ProjectManager::operationError, [&](QString msg) {
        QMessageBox::critical(_parentUI, "保存出错", msg);
        loop.quit();
        });

    ProjectManager::instance()->saveProject();
    loop.exec();

    disconnect(con1);
    disconnect(con2);

    ProjectManager::instance()->setDirty(!isOk);
    if (isOk) {
        Q_EMIT projectSavedSuccessfully(ProjectManager::instance()->currentFilePath());
    }
    return isOk;
}

bool ProjectActionController::handleSaveAsProject() {
    QString defaultName = ProjectSetting::instance()->_projectName;
    if (defaultName.isEmpty()) defaultName = "未命名项目";
    QString filePath = QFileDialog::getSaveFileName(_parentUI, "另存为", defaultName, "RabbitRoll (*.rroll)");
    if (filePath.isEmpty()) return false;

    QFileInfo fileInfo(filePath);
    ProjectSetting::instance()->_projectName = fileInfo.completeBaseName();

    QProgressDialog progress("正在努力另存为...", QString(), 0, 0, _parentUI);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    QProgressBar* bar = progress.findChild<QProgressBar*>();
    bar->setAlignment(Qt::AlignCenter);

    QEventLoop loop;
    bool isOk = false;

    QMetaObject::Connection con1 = connect(ProjectManager::instance(), &ProjectManager::saveFinished, [&](bool ok) {
        isOk = ok;
        loop.quit();
        });
    QMetaObject::Connection con2 = connect(ProjectManager::instance(), &ProjectManager::operationError, [&](QString msg) {
        QMessageBox::critical(_parentUI, "另存为失败", msg);
        isOk = false;
        loop.quit();
        });

    ProjectManager::instance()->saveProjectAs(filePath);
    loop.exec();

    disconnect(con1);
    disconnect(con2);

    if (isOk) {
        Q_EMIT projectSavedSuccessfully(filePath);
    }
    return isOk;
}

void ProjectActionController::handleExportScheduleExcel() {
    QString defaultName = ProjectSetting::instance()->_projectName;
    if (defaultName.isEmpty()) defaultName = "未命名项目";

    QString filePath = QFileDialog::getSaveFileName(_parentUI, "导出排班表为Excel", defaultName + "_排班表", "Excel 工作簿 (*.xlsx)");
    if (filePath.isEmpty()) return;

    QList<ExportTableData> exportDataList;

    auto* editorComp = ComponentManager::instance()->getComponent<EditorDutyComponent>();
    if (editorComp && editorComp->getModel()) {
        auto* eModel = editorComp->getModel();
        if (eModel->rowCount() > 0 && eModel->columnCount() > 0) {
            ExportTableData eData;
            eData.sheetName = "总排班表";
            for (int c = 0; c < eModel->columnCount(); ++c) {
                eData.horizontalHeaders << eModel->headerData(c, Qt::Horizontal).toString();
            }
            for (int r = 0; r < eModel->rowCount(); ++r) {
                eData.verticalHeaders << eModel->headerData(r, Qt::Vertical).toString();
                QStringList rowData;
                for (int c = 0; c < eModel->columnCount(); ++c) {
                    QString cellText = eModel->data(eModel->index(r, c), Qt::DisplayRole).toString();
                    cellText.replace("\n", ", ");
                    rowData << cellText;
                }
                eData.cells << rowData;
            }
            exportDataList << eData;
        }
    }

    if (exportDataList.isEmpty()) {
        QMessageBox::warning(_parentUI, "提示", "排班表中没有可导出的数据！");
        return;
    }

    QProgressDialog progress("正在导出排班表为Excel...", QString(), 0, 0, _parentUI);
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    progress.show();
    QProgressBar* bar = progress.findChild<QProgressBar*>();
    bar->setAlignment(Qt::AlignCenter);

    QEventLoop loop;
    bool exportOk = false;
    QString errMsg;

    QMetaObject::Connection connFinished = connect(BackendWorker::instance(), &BackendWorker::excelExportFinished,
        [&](bool success, const QString& errorMsg) {
            exportOk = success;
            errMsg = errorMsg;
            loop.quit();
        });

    Q_EMIT requestExportExcel(filePath, exportDataList);
    loop.exec();
    disconnect(connFinished);

    if (exportOk) QMessageBox::information(_parentUI, "导出成功", "排班表已成功导出！");
    else QMessageBox::critical(_parentUI, "导出失败", errMsg);
}

void ProjectActionController::handleExportMembersExcel() {
    QString defaultName = ProjectSetting::instance()->_projectName;
    if (defaultName.isEmpty()) defaultName = "未命名项目";

    QString filePath = QFileDialog::getSaveFileName(_parentUI, "导出成员列表为Excel", defaultName + "_成员列表", "Excel 工作簿 (*.xlsx)");
    if (filePath.isEmpty()) return;

    QList<ExportTableData> exportDataList;

    auto* mComp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    if (mComp && mComp->getModel()) {
        auto* mModel = mComp->getModel();
        if (mModel->rowCount() > 0 && mModel->columnCount() > 0) {
            ExportTableData mData;
            mData.sheetName = "成员信息";

            for (int c = 0; c < mModel->columnCount(); ++c) {
                mData.horizontalHeaders << mModel->headerData(c, Qt::Horizontal).toString();
            }

            for (int r = 0; r < mModel->rowCount(); ++r) {
                mData.verticalHeaders << mModel->headerData(r, Qt::Vertical).toString();

                QStringList rowData;
                for (int c = 0; c < mModel->columnCount(); ++c) {
                    QString cellText = mModel->data(mModel->index(r, c), Qt::DisplayRole).toString();
                    cellText.replace("\n", ", ");
                    rowData << cellText;
                }
                mData.cells << rowData;
            }
            exportDataList << mData;
        }
    }

    if (exportDataList.isEmpty()) {
        QMessageBox::warning(_parentUI, "提示", "成员列表中没有可导出的数据！");
        return;
    }

    QProgressDialog progress("正在导出成员列表为Excel...", QString(), 0, 0, _parentUI);
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    progress.show();
    QProgressBar* bar = progress.findChild<QProgressBar*>();
    bar->setAlignment(Qt::AlignCenter);

    QEventLoop loop;
    bool exportOk = false;
    QString errMsg;

    QMetaObject::Connection connFinished = connect(BackendWorker::instance(), &BackendWorker::excelExportFinished,
        [&](bool success, const QString& errorMsg) {
            exportOk = success;
            errMsg = errorMsg;
            loop.quit();
        });

    Q_EMIT requestExportExcel(filePath, exportDataList);
    loop.exec();
    disconnect(connFinished);

    if (exportOk) QMessageBox::information(_parentUI, "导出成功", "成员列表已成功导出！");
    else QMessageBox::critical(_parentUI, "导出失败", errMsg);
}

void ProjectActionController::handleExportMembersData() {
    QString defaultName = "未命名成员名单";
    if (!ProjectSetting::instance()->_projectName.isEmpty()) {
        defaultName = ProjectSetting::instance()->_projectName + "_成员";
    }

    QString filePath = QFileDialog::getSaveFileName(_parentUI, "导出成员数据", defaultName, "RabbitRoll Member (*.rmem)");
    if (filePath.isEmpty()) return;

    QProgressDialog progress("正在努力导出成员数据...", QString(), 0, 0, _parentUI);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    QProgressBar* bar = progress.findChild<QProgressBar*>();
    bar->setAlignment(Qt::AlignCenter);

    QEventLoop loop;
    bool isOk = false;

    QMetaObject::Connection connSuccess = connect(ProjectManager::instance(), &ProjectManager::exportFinished, [&](bool ok) {
        isOk = ok;
        loop.quit();
        });
    QMetaObject::Connection connError = connect(ProjectManager::instance(), &ProjectManager::operationError, [&](QString msg) {
        QMessageBox::critical(_parentUI, "导出失败", msg);
        isOk = false;
        loop.quit();
        });

    ProjectManager::instance()->exportMembersAs(filePath);
    loop.exec();

    disconnect(connSuccess);
    disconnect(connError);
}

void ProjectActionController::handleExportSchedule() {
    QString defaultName = ProjectSetting::instance()->_projectName;
    if (defaultName.isEmpty()) defaultName = "未命名排班";

    QString filePath = QFileDialog::getSaveFileName(_parentUI, "导出排班数据", defaultName + "_排班数据", "RabbitRoll Schedule (*.rsch)");
    if (filePath.isEmpty()) return;

    auto* sComp = ComponentManager::instance()->getComponent<ScheduleComponent>();
    if (!sComp || !sComp->getModel()) {
        QMessageBox::warning(_parentUI, "提示", "未找到有效的排班模型！");
        return;
    }

    QJsonObject dataToExport = sComp->getModel()->exportScheduleData();

    QProgressDialog progress("正在导出排班数据...", QString(), 0, 0, _parentUI);
    progress.setWindowModality(Qt::WindowModal);
    progress.setCancelButton(nullptr);
    progress.show();
    QProgressBar* bar = progress.findChild<QProgressBar*>();
    bar->setAlignment(Qt::AlignCenter);

    QEventLoop loop;
    bool isOk = false;

    QMetaObject::Connection con1 = connect(ProjectManager::instance(), &ProjectManager::scheduleExportFinished, [&](bool ok) {
        isOk = ok;
        loop.quit();
        });

    QMetaObject::Connection con2 = connect(ProjectManager::instance(), &ProjectManager::operationError, [&](QString msg) {
        QMessageBox::critical(_parentUI, "导出出错", msg);
        isOk = false;
        loop.quit();
        });

    ProjectManager::instance()->exportScheduleAs(filePath, dataToExport);
    loop.exec();

    disconnect(con1);
    disconnect(con2);

    if (isOk) {
        /*QMessageBox::information(_parentUI, "成功", "排班数据导出成功！");*/
    }
}

void ProjectActionController::handleImportMembersData() {
    QString filePath = QFileDialog::getOpenFileName(_parentUI, "导入成员列表", "", "RabbitRoll Member (*.rmem);");
    if (filePath.isEmpty()) return;

    QProgressDialog progress("正在导入成员数据...", QString(), 0, 0, _parentUI);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();
    QProgressBar* bar = progress.findChild<QProgressBar*>();
    bar->setAlignment(Qt::AlignCenter);

    QEventLoop loop;
    bool isSuccess = false;

    QMetaObject::Connection conn = connect(ProjectManager::instance(), &ProjectManager::importMembersFinished,
        [&](bool success) {
            isSuccess = success;
            loop.quit();
        });

    ProjectManager::instance()->importMembersAsync(filePath);

    loop.exec();
    disconnect(conn);
    progress.close();

    if (isSuccess) {
        Q_EMIT ImportMembersDataSuccessfully();
    }
    else {
        QMessageBox::critical(_parentUI, "导入失败", "解析成员数据文件时出错。");
    }
}
