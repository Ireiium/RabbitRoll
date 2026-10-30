#pragma once

#include <QString>
#include <QStringList>

namespace ExcelTemplates {
QString directory();
QString defaultFileName();
QStringList fileNames();
QString selectedFilePath(QString& error);
}
