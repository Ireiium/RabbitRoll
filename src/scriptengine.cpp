#include "scriptengine.h"
#include "scriptapi.h"
#include <pybind11/embed.h>
#include <QFile>
#include <QDebug>
#include <set>
#include <QCoreApplication>
#include <QDir>
#include "componentmanager.h"

namespace py = pybind11;

static const std::set<std::string> IMPORT_WHITELIST = {
    "random", "math", "datetime", "json", "copy", "re", "rabbit_api"
};

ScriptEngine::ScriptEngine(QObject* parent) : QObject(parent) {}
ScriptEngine::~ScriptEngine() {
}

ScriptEngine* ScriptEngine::instance() {
    static ScriptEngine engine;
    return &engine;
}

void ScriptEngine::init() {
    if (_isInitialized) return;
    _canScriptWork = false;
    QString appDir = QCoreApplication::applicationDirPath();
    QString envPath = appDir + "/pythonembed";
    QDir dir(envPath);

    if (!dir.exists()) {
        qWarning() << "Python环境文件夹pythonembed缺失。";
        envPath = appDir;
    }

#if defined(Q_OS_WIN)
	//环境根目录+压缩包标准库+解压版标准库+扩展DLLs
    QString pythonPath = envPath + ";" + envPath + "/python312.zip;" + envPath + "/Lib;" + envPath + "/DLLs";

    std::wstring wstrPath = pythonPath.toStdWString();
    Py_SetPath(wstrPath.c_str());

    //qDebug() << "Python已设置为:" << pythonPath;

#endif
    try {
        py::initialize_interpreter();
        _isInitialized = true;
        _gilRelease = new py::gil_scoped_release();
        _canScriptWork = true;
        qDebug() << "Python成功初始化.";
    } catch (const std::exception& e) {
        _canScriptWork = false;
        qCritical() << "Python 初始化失败" << e.what();
    }
}

QHash<QString, QList<QUuid>> ScriptEngine::runScheduleScript(
    const QString& scriptPath,
    int targetPerCell,
    const QHash<QString, QStringList>& taskCandidates,
    const QList<MemberInfoModel::MemberData>& allMembersData,
    QString& debugMsg)
{
    qDebug() << "运行脚本" << scriptPath;
    QHash<QString, QList<QUuid>> schedule;
    if (!_isInitialized) {
        debugMsg = "Python 引擎未初始化";
        return schedule;
    }

    py::gil_scoped_acquire acquire;

    try {
        std::map<std::string, std::vector<std::string>> apiTaskData;
        for (auto it = taskCandidates.begin(); it != taskCandidates.end(); ++it) {
            std::vector<std::string> members;
            for (const QString& uid : it.value()) {
                members.push_back(uid.toStdString());
            }
            apiTaskData[it.key().toStdString()] = members;
        }

        std::vector<std::string> apiAllMembers;
        std::map<std::string, std::map<std::string, std::string>> apiMemberProps;
        std::map<std::string, int> apiMemberDutyCounts;
        std::map<std::string, std::map<std::string, std::map<std::string, bool>>> apiMemberSchedules;

        for (const auto& member : allMembersData) {
            std::string uid = member.id.toString().toStdString();
            apiAllMembers.push_back(uid);

            std::map<std::string, std::string> props;
            for (auto it = member.properties.begin(); it != member.properties.end(); ++it) {
                props[it.key().toStdString()] = it.value().toString().toStdString();
            }
            apiMemberProps[uid] = props;
            apiMemberDutyCounts[uid] = member.dutyCount;

            std::map<std::string, std::map<std::string, bool>> schDateMap;
            for (auto dIt = member.scheduleMap.begin(); dIt != member.scheduleMap.end(); ++dIt) {
                std::map<std::string, bool> schTimeMap;
                for (auto tIt = dIt.value().begin(); tIt != dIt.value().end(); ++tIt) {
                    schTimeMap[tIt.key().toStdString()] = tIt.value();
                }
                schDateMap[dIt.key().toStdString()] = schTimeMap;
            }
            apiMemberSchedules[uid] = schDateMap;
        }

        ScriptAPI api;
        api.setAvailableData(apiTaskData);
        api.setAllMembers(apiAllMembers);
        api.setMemberProperties(apiMemberProps);
        api.setMemberDutyCounts(apiMemberDutyCounts);
        api.setMemberSchedules(apiMemberSchedules);

        //沙盒
        py::dict globals;
        py::dict locals;

        py::module_ builtins_mod = py::module_::import("builtins");
        py::dict safe_builtins;

        py::object real_import = builtins_mod.attr("__import__");

        auto safe_import_func = [real_import](py::str name, py::object globals, py::object locals, py::object fromlist, int level) {
            std::string module_name = name.cast<std::string>();
            if (IMPORT_WHITELIST.find(module_name) == IMPORT_WHITELIST.end()) {
                throw py::import_error("Security Error: module '" + module_name + "' is restricted.");
            }
            return real_import(name, globals, locals, fromlist, level);
            };

        for (auto item : builtins_mod.attr("__dict__").cast<py::dict>()) {
            std::string key = py::cast<std::string>(item.first);
            if (key == "__import__") {
                safe_builtins[item.first] = py::cpp_function(safe_import_func);
                continue;
            }
            if (key == "eval" || key == "exec" || key == "open" ||
                key == "compile" || key == "globals" || key == "locals" ||
                key == "getattr" || key == "setattr" || key == "delattr") {
                continue;
            }
            safe_builtins[item.first] = item.second;
        }

        globals["__builtins__"] = safe_builtins;

        QFile scriptFile(scriptPath);
        if (!scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            debugMsg = "无法读取脚本文件";
            return schedule;
        }
        std::string scriptCode = scriptFile.readAll().toStdString();

        py::module_::import("rabbit_api");
        locals["api"] = py::cast(&api);
        locals["target_per_cell"] = targetPerCell;

        //用户可以不写下面这些import
        globals["random"] = py::module_::import("random");
        globals["datetime"] = py::module_::import("datetime");

        auto* mComp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
        auto* undoStack = mComp->getController()->getUndoStack();

        api.resetModified();
        undoStack->beginMacro("PythonChangedData");

        py::exec(scriptCode, globals, locals);

        if (!locals.contains("generate_schedule")) {
            debugMsg = "脚本中未定义规定的入口函数: generate_schedule(api, target_per_cell)";
            return schedule;
        }

        py::function generateFunc = locals["generate_schedule"];
        py::object pyResult = generateFunc(locals["api"], targetPerCell);

        try {
            py::dict pyResultDict = pyResult.cast<py::dict>();

            for (auto item : pyResultDict) {
                QString taskKey = QString::fromStdString(item.first.cast<std::string>());
                QList<QUuid> uids;

                try {
                    py::list pyList = item.second.cast<py::list>();

                    for (auto pyUid : pyList) {
                        std::string uidStr = pyUid.cast<std::string>();
                        uids.append(QUuid(QString::fromStdString(uidStr)));
                    }
                }
                catch (const std::exception& e) {
                    undoStack->endMacro();
                    qDebug() << "警告：任务" << taskKey << "的值不是字符串列表，具体错误:" << e.what();
                }

                schedule.insert(taskKey, uids);
            }
        }
        catch (const std::exception& e) {
            undoStack->endMacro();
            debugMsg = QString("数据格式转换异常: %1").arg(e.what());
            return schedule;
        }
        undoStack->endMacro();
        if (api.hasModified()) {
            Q_EMIT scriptChangedData(true);
        }
    }
    catch (const py::error_already_set& e) {

        debugMsg = QString("Python 脚本执行出错:\n%1").arg(e.what());
        qDebug() << debugMsg;
    }
    catch (const std::exception& e) {
        debugMsg = QString("C++ 转换/执行异常:\n%1").arg(e.what());
        qDebug() << debugMsg;
    }

    return schedule;
}
