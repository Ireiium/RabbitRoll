#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <pybind11/pybind11.h>
#include "memberinfomodel.h"

class ScriptEngine : public QObject {
    Q_OBJECT

Q_SIGNALS:
    void scriptChangedData(bool isChnaged);

public:
    static ScriptEngine* instance();
    void init();

    QHash<QString, QList<QUuid>> runScheduleScript(
        const QString& scriptPath,
        int targetPerCell,
        const QHash<QString, QStringList>& taskCandidates,
        const QList<MemberInfoModel::MemberData>& allMembersData,
        QString& debugMsg
    );

    bool _canScriptWork{ false };

private:
    ScriptEngine(QObject* parent = nullptr);
    ~ScriptEngine();

    bool _isInitialized = false;
    pybind11::gil_scoped_release* _gilRelease = nullptr;
};
