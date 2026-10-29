#include "settings.h"
#include "lock.h"
#include <QtGlobal>

Settings* Settings::instance()
{
    static Settings instance;
    return &instance;
}

Settings::Settings(QObject* parent)
    : QObject(parent)
{
    m_settings = new QSettings("Ireiium", "RabbitRoll", this);
}

Settings::~Settings()
{
}

void Settings::sync()
{
    m_settings->sync();
}

//最近文件设置
QStringList Settings::getRecentFiles() const
{
    return m_settings->value("RecentFiles").toStringList();
}

void Settings::setRecentFiles(const QStringList& files)
{
    m_settings->setValue("RecentFiles", files);
}

void Settings::removeRecentFile(const QString& filePath)
{
    QStringList files = getRecentFiles();
    if (files.removeAll(filePath) > 0) {
        setRecentFiles(files);
        sync();
    }
}

void Settings::addRecentFile(const QString& filePath)
{
    QStringList files = getRecentFiles();
    files.removeAll(filePath);
    files.prepend(filePath);

    if (files.size() > 15) {
        files.removeLast();
    }
    setRecentFiles(files);
}

//脚本设置
bool Settings::getEnableScript() const { 
    return m_settings->value("EnableScript", false).toBool(); 
}
void Settings::setEnableScript(bool enable) { 
    m_settings->setValue("EnableScript", enable); 
}

QString Settings::getDefaultScript() const { 
    return m_settings->value("DefaultScript", "").toString(); 
}
void Settings::setDefaultScript(const QString& scriptName) { 
    m_settings->setValue("DefaultScript", scriptName); 
}
bool Settings::getEnableConsole() const { 
    return m_settings->value("EnableConsole", false).toBool();
}
void Settings::setEnableConsole(bool enable) { 
    m_settings->setValue("EnableConsole", enable);
}

//UI和窗口设置
int Settings::getThemeMode() const { 
    return m_settings->value("ThemeMode", 0).toInt(); }
void Settings::setThemeMode(int mode) {
    m_settings->setValue("ThemeMode", mode); 
}

int Settings::getWindowMode() const { 
    return m_settings->value("WindowMode", 0).toInt(); 
}
void Settings::setWindowMode(int mode) { 
    m_settings->setValue("WindowMode", mode); 
}
int Settings::getNavMode() const { 
    return m_settings->value("NavMode", 2).toInt(); 
}
void Settings::setNavMode(int mode) {
    m_settings->setValue("NavMode", mode);
}


// AI设置
#ifdef ENABLE_AI_SUPPORT
bool Settings::getEnableAI() const {
    return m_settings->value("EnableAI", false).toBool();
}
void Settings::setEnableAI(bool enable) {
    m_settings->setValue("EnableAI", enable);
}
QString Settings::getAIModel() const {
    return m_settings->value("AIModel", "").toString();
}
void Settings::setAIModel(const QString& modelName) {
    m_settings->setValue("AIModel", modelName);
}

QString Settings::getAPI() const {
    QString key = qEnvironmentVariable("RABBITROLL_DEEPSEEK_API_KEY").trimmed();
    if (key.isEmpty()) {
        key = qEnvironmentVariable("DEEPSEEK_API_KEY").trimmed();
    }
    if (!key.isEmpty()) {
        return key;
    }

    const QByteArray encrypted = m_settings->value("AIApiKeyEncrypted").toByteArray();
    return QString::fromUtf8(deobfuscateKey(encrypted)).trimmed();
}

void Settings::setAPI(const QString& key) {
    const QString trimmedKey = key.trimmed();
    if (trimmedKey.isEmpty()) {
        m_settings->remove("AIApiKeyEncrypted");
    } else {
        m_settings->setValue("AIApiKeyEncrypted", obfuscateKey(trimmedKey.toUtf8()));
    }
}
#endif



void Settings::clearAllSettings()
{
	QStringList recent = getRecentFiles();
    m_settings->clear();
    setRecentFiles(recent);
    sync();
}