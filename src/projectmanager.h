#pragma once

#include <QObject>
#include <QJsonObject>

class ProjectManager : public QObject
{
    Q_OBJECT

Q_SIGNALS:
    void projectStatusChanged();
    void projectLoaded();
    void operationError(const QString& msg);
    void saveFinished(bool success);
    void exportFinished(bool success);
    void importMembersFinished(bool success);
    void scheduleImportFinished(bool success , QJsonObject obj);
    void scheduleExportFinished(bool success);

public:
    enum class SaveStatus { Success, NeedPath, IOError };
    static ProjectManager* instance();
    ~ProjectManager() override;

    bool isDirty() const;
    void setDirty(bool dirty);
    QString currentFilePath() const;

    bool newProject(const QString& name);
    void openProject(const QString& filePath);

    void saveProject();
    void saveProjectAs(const QString& filePath);

    SaveStatus checkSaveStatus() const;

    void exportMembersAs(const QString& filePath);
    void importMembersAsync(const QString& filePath);

    void importScheduleFromFile(const QString& filePath, const QDate& startDate);
    void exportScheduleAs(const QString& filePath, const QJsonObject& scheduleData);

private:
    explicit ProjectManager(QObject* parent = nullptr);

    QString version;
    bool _isDirty{ false };
    QString _currentFilePath;
    quint64 _changeRevision{ 0 };
    quint64 _projectGeneration{ 0 };

    QJsonObject serializeData();
    void deserializeData(const QJsonObject& doc);

    void deserializeMemberData(const QJsonObject& memberSection);
};
