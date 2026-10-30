#include "exceltemplatepicker.h"
#include "exceltemplates.h"
#include "settings.h"
#include "ElaComboBox.h"
#include "ElaMessageBar.h"
#include "ElaText.h"
#include "ElaTheme.h"

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QDir>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QPalette>
#include <QSignalBlocker>
#include <QShowEvent>
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
    connect(eTheme, &ElaTheme::themeModeChanged, this, &ExcelTemplatePicker::updateTemplateColors);
    updateTemplateColors();
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
    auto selected = Settings::instance()->getExcelTemplate();
    auto matchingName = [&](const QString& name) {
        for (const auto& file : available)
            if (file.compare(name, Qt::CaseInsensitive) == 0) return file;
        return QString{};
    };
    selected = matchingName(selected);
    if (selected.isEmpty()) selected = matchingName(ExcelTemplates::defaultFileName());
    if (selected.isEmpty() && !available.isEmpty()) selected = available.first();
    if (Settings::instance()->getExcelTemplate() != selected)
        Settings::instance()->setExcelTemplate(selected);
    for (const auto& name : available) {
        QString label = name;
        if (name.compare(ExcelTemplates::defaultFileName(), Qt::CaseInsensitive) == 0) label += "（默认）";
        _templates->addItem(label, name);
    }
    _templates->addItem("打开模板文件夹…");
    _templates->setItemData(_templates->count() - 1, true, Qt::UserRole + 1);
    _templates->setCurrentIndex(selected.isEmpty() ? _templates->count() - 1 : _templates->findData(selected));
    QString error;
    const auto path = ExcelTemplates::selectedFilePath(error);
    _templates->setToolTip(error.isEmpty() ? QDir::toNativeSeparators(path) : error);
    updateTemplateColors();
}

void ExcelTemplatePicker::updateTemplateColors()
{
    const auto mode = eTheme->getThemeMode();
    const auto text = ElaThemeColor(mode, BasicText);
    const auto disabledText = ElaThemeColor(mode, BasicTextDisable);
    auto palette = _templates->palette();
    for (auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        const auto color = group == QPalette::Disabled ? disabledText : text;
        for (auto role : {QPalette::Text, QPalette::ButtonText, QPalette::WindowText, QPalette::HighlightedText})
            palette.setColor(group, role, color);
        palette.setColor(group, QPalette::PlaceholderText, disabledText);
    }
    _templates->setPalette(palette);
    _templates->view()->setPalette(palette);
    _templates->update();
    _templates->view()->viewport()->update();
}

void ExcelTemplatePicker::showEvent(QShowEvent* event)
{
    reloadTemplates();
    QWidget::showEvent(event);
}
