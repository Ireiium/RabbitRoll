#pragma once

#include <QString>

class ApiKeyStore
{
public:
    explicit ApiKeyStore(const QString& filePath);
    bool load(QString& key, QString& error) const;
    bool save(const QString& key, QString& error) const;

private:
    QString _filePath;
};
