#include "exceltemplatepicker.h"
#include "exceltemplates.h"
#include "settings.h"
#include "ElaComboBox.h"
#include "ElaMessageBar.h"
#include "ElaText.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QStandardItemModel>
#include <QUrl>

ExcelTemplatePicker::ExcelTemplatePicker(QWidget* parent) : ElaScrollPageArea(parent)
{
    setObjectName("excelTemplatePicker");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(15, 5, 15, 5);
    auto* label = new ElaText("Excel导出模板：", 18, this);
    label->setObjectName("excelTemplateLabel");
    layout->addWidget(label);
    layout->addStretch();
    _templates = new ElaComboBox(this);
    _templates->setObjectName("excelTemplateCombo");
    _templates->setFixedWidth(300);
    layout->addWidget(_templates);
    connect(_templates, &QComboBox::activated, this, [this](int index) {
        if (_templates->itemData(index, Qt::UserRole + 1).toBool()) {
            // The folder shortcut is an action; keep the saved template selected.
            reloadTemplates();
            const auto path = ExcelTemplates::directory();
            if (!QDir().mkpath(path) || !QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
                ElaMessageBar::error(ElaMessageBarType::BottomLeft, "打开失败", "无法打开模板文件夹", 3000, this);
            reloadTemplates();
            return;
        }
        const auto name = _templates->itemData(index).toString();
        if (!ExcelTemplates::fileNames().contains(name, Qt::CaseInsensitive)) {
            reloadTemplates();
            return;
        }
        Settings::instance()->setExcelTemplate(name);
        QString error;
        const auto path = ExcelTemplates::selectedFilePath(error);
        _templates->setToolTip(error.isEmpty() ? QDir::toNativeSeparators(path) : error);
    });
    QDir().mkpath(ExcelTemplates::directory());
    _watcher = new QFileSystemWatcher(this);
    connect(_watcher, &QFileSystemWatcher::directoryChanged, this, &ExcelTemplatePicker::reloadTemplates);
    reloadTemplates();
}

void ExcelTemplatePicker::reloadTemplates()
{
    if (QDir(ExcelTemplates::directory()).exists() && !_watcher->directories().contains(ExcelTemplates::directory()))
        _watcher->addPath(ExcelTemplates::directory());
    const QSignalBlocker blocker(_templates);
    _templates->clear();
    const auto available = ExcelTemplates::fileNames();
    const auto selected = Settings::instance()->getExcelTemplate();
    auto names = available;
    if (!names.contains(ExcelTemplates::defaultFileName(), Qt::CaseInsensitive))
        names.prepend(ExcelTemplates::defaultFileName());
    if (!selected.isEmpty() && !names.contains(selected, Qt::CaseInsensitive)) names.append(selected);
    for (const auto& name : names) {
        const bool exists = available.contains(name, Qt::CaseInsensitive);
        QString label = name;
        if (name.compare(ExcelTemplates::defaultFileName(), Qt::CaseInsensitive) == 0) label += "（默认）";
        if (!exists) label += "（未找到文件）";
        _templates->addItem(label, name);
        if (!exists) {
            if (auto* model = qobject_cast<QStandardItemModel*>(_templates->model()))
                model->item(_templates->count() - 1)->setEnabled(false);
        }
        if (name.compare(selected, Qt::CaseInsensitive) == 0)
            _templates->setCurrentIndex(_templates->count() - 1);
    }
    _templates->addItem("打开模板文件夹…");
    _templates->setItemData(_templates->count() - 1, true, Qt::UserRole + 1);
    QString error;
    const auto path = ExcelTemplates::selectedFilePath(error);
    _templates->setToolTip(error.isEmpty() ? QDir::toNativeSeparators(path) : error);
}

void ExcelTemplatePicker::showEvent(QShowEvent* event)
{
    reloadTemplates();
    QWidget::showEvent(event);
}
