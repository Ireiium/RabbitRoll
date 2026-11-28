#pragma once

#include <QObject>
#include <QJsonObject>
#include <QHash>
#include <QUuid>

class ProjectManager : public QObject
{
    Q_OBJECT

Q_SIGNALS:
    void projectStatusChanged();
    void projectLoaded();
    void projectClosed();
    void projectFileSaved(const QString& filePath);
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
    bool hasProject() const { return !_sessionId.isEmpty(); }
    bool isLoading() const { return _pendingLoads > 0; }
    bool isSaving() const { return _pendingSaves > 0; }
    QJsonObject projectSnapshot();
    QJsonObject memberExportSnapshot() const;
    bool acceptSavedSnapshot(const QString& path, const QString& session, quint64 revision);
    QString projectSessionId() const { return _sessionId; }
    quint64 changeRevision() const { return _changeRevision; }

    bool newProject(const QString& name);
    void closeProject();
    static bool validProject(const QJsonObject& root);
    static bool validMembers(const QJsonObject& root);
    bool replaceProject(const QJsonObject& root, const QString& path);
    bool restoreMemberState(const QJsonObject& section, const QHash<QString,QList<QUuid>>& assignments);
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
    int _pendingLoads{0};
    int _pendingSaves{0};
    QString _sessionId;

    QJsonObject serializeData();
    QJsonObject serializeMemberData() const;
    void deserializeData(const QJsonObject& doc);

    void deserializeMemberData(const QJsonObject& memberSection);
};
