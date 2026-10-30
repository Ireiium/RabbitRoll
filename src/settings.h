#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QString>

class Settings : public QObject
{
    Q_OBJECT

public:
    static Settings* instance();

    void sync();

    QStringList getRecentFiles() const;
    void setRecentFiles(const QStringList& files);
    void removeRecentFile(const QString& filePath);
    void addRecentFile(const QString& filePath);

    bool getEnableScript() const;
    void setEnableScript(bool enable);
    QString getDefaultScript() const;
    void setDefaultScript(const QString& scriptName);
    bool getEnableConsole() const;
    void setEnableConsole(bool enable);
    QString getExcelTemplate() const;
    void setExcelTemplate(const QString& fileName);

    int getThemeMode() const;
    void setThemeMode(int mode);
    int getWindowMode() const;
    void setWindowMode(int mode);
    int getNavMode() const;
    void setNavMode(int mode);

    void clearAllSettings();

    //AI设置
#ifdef ENABLE_AI_SUPPORT
    bool getEnableAI() const;
    void setEnableAI(bool enable);
    QString getAIModel() const;
    void setAIModel(const QString& modelName);
    QString getAPI(QString* error = nullptr) const;
    bool setAPI(const QString& key, QString* error = nullptr);
    QString apiKeyFilePath() const;
#endif

private:
    explicit Settings(QObject* parent = nullptr);
    ~Settings();
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    QSettings* m_settings;
#ifdef ENABLE_AI_SUPPORT
    QString _apiKeyFilePath;
#endif
};
