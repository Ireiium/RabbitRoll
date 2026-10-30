#include "exceltemplates.h"
#include "settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

QString ExcelTemplates::directory()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath("Excel template");
}

QString ExcelTemplates::defaultFileName()
{
    return QStringLiteral("飞书排班模板.xlsx");
}

QStringList ExcelTemplates::fileNames()
{
    QDir folder(directory());
    QStringList result;
    for (const auto& entry : folder.entryInfoList({"*.xlsx"}, QDir::Files | QDir::Readable | QDir::NoSymLinks, QDir::Name)) {
        if (!entry.fileName().startsWith("~$")
            && entry.canonicalPath().compare(folder.canonicalPath(), Qt::CaseInsensitive) == 0)
            result.append(entry.fileName());
    }
    return result;
}

QString ExcelTemplates::selectedFilePath(QString& error)
{
    error.clear();
    const auto name = Settings::instance()->getExcelTemplate();
    if (name.isEmpty() || name.contains('/') || name.contains('\\') || QFileInfo(name).isAbsolute()
        || QFileInfo(name).suffix().compare("xlsx", Qt::CaseInsensitive) != 0) {
        error = "模板设置无效，请在“设置 → 导出设置”中选择指定文件夹内的 .xlsx 文件。";
        return {};
    }
    if (!fileNames().contains(name, Qt::CaseInsensitive)) {
        error = QString("未找到模板“%1”。请放入以下目录，再到“设置 → 导出设置”选择：\n%2")
            .arg(name, QDir::toNativeSeparators(directory()));
        return {};
    }
    const auto file = QFileInfo(QDir(directory()).filePath(name));
    if (!file.isFile() || !file.isReadable() || file.isSymLink()
        || file.canonicalPath().compare(QDir(directory()).canonicalPath(), Qt::CaseInsensitive) != 0) {
        error = "只能读取 Excel template 文件夹内的模板文件，不能读取外部链接。";
        return {};
    }
    return file.canonicalFilePath();
}
