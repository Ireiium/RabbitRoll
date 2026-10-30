#include <QString>
#include <QStringList>
#include <QHash>
#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include "scriptapi.h"
#include "rconsole.h"
#include <stdexcept>

namespace py = pybind11;

//注入
void ScriptAPI::setAvailableData(const std::map<std::string, std::vector<std::string>>& candidates) {
    _taskCandidates = candidates;
}
void ScriptAPI::setAllMembers(const std::vector<std::string>& members) { 
    _allMembers = members; 
}
void ScriptAPI::setMemberProperties(const std::map<std::string, std::map<std::string, std::string>>& props) { 
    _memberProperties = props;
}
void ScriptAPI::setMemberDutyCounts(const std::map<std::string, int>& counts) {
    _memberDutyCounts = counts; 
}
void ScriptAPI::setMemberSchedules(const std::map<std::string, std::map<std::string, std::map<std::string, bool>>>& schedules) { 
    _memberSchedules = schedules; 
}

std::string ScriptAPI::findMemberByName(const std::string& name, int index) const {
    int matchCount = 0;
    for (const std::string& uid : _allMembers) {
        auto it = _memberProperties.find(uid);
        if (it != _memberProperties.end()) {
            const auto& props = it->second;
            auto nameIt = props.find("姓名");
            if (nameIt != props.end() && nameIt->second == name) {
                if (matchCount == index) {
                    return uid;
                }
                matchCount++;
            }
        }
    }
    return "";
}

void ScriptAPI::debug(const std::string& msg) const {
    QString qmsg = QString::fromStdString(msg);
    if (RConsole::isInstantiated()) {
        QMetaObject::invokeMethod(RConsole::instance(), "appendLog",
            Qt::QueuedConnection, Q_ARG(QString, qmsg));
    }
    else {
        qDebug() << "[Python]:" << qmsg;
    }
}

void ScriptAPI::setMemberPropertyToModel(const std::string& memberId, const std::string& key, const std::string& value) {
    auto member = _memberProperties.find(memberId);
    if (member == _memberProperties.end()) {
        throw std::invalid_argument("Unknown member ID");
    }
    const auto property = member->second.find(key);
    if (property != member->second.end() && property->second == value) return;
    member->second[key] = value;
    _pendingProperties[memberId][key] = value;
    _isModified = true;
}


//接口
std::map<std::string, std::vector<std::string>> ScriptAPI::getTaskCandidates() const { return _taskCandidates; }
std::vector<std::string> ScriptAPI::getAllMembers() const { return _allMembers; }

std::map<std::string, std::string> ScriptAPI::getMemberProperties(const std::string& memberId) const {
    auto it = _memberProperties.find(memberId);
    if (it != _memberProperties.end()) return it->second;
    return {};
}

int ScriptAPI::getMemberDutyCount(const std::string& memberId) const {
    auto it = _memberDutyCounts.find(memberId);
    if (it != _memberDutyCounts.end()) return it->second;
    return 0;
}

std::map<std::string, std::map<std::string, bool>> ScriptAPI::getMemberSchedule(const std::string& memberId) const {
    auto it = _memberSchedules.find(memberId);
    if (it != _memberSchedules.end()) return it->second;
    return {};
}

std::map<std::string, int> ScriptAPI::getMemberDutyCounts() const {
    return _memberDutyCounts;
}

PYBIND11_EMBEDDED_MODULE(rabbit_api, m) {
    py::class_<ScriptAPI>(m, "ScriptAPI")
        .def("get_task_candidates", &ScriptAPI::getTaskCandidates, "获取任务候选人数据")
        .def("get_all_members", &ScriptAPI::getAllMembers, "获取所有成员 ID 列表")
        .def("get_member_properties", &ScriptAPI::getMemberProperties, "获取成员属性")
        .def("get_member_duty_count", &ScriptAPI::getMemberDutyCount, "获取单个成员当前值勤次数")
        .def("get_member_duty_counts", &ScriptAPI::getMemberDutyCounts, "获取所有成员值勤次数映射表")
        .def("debug", &ScriptAPI::debug, py::arg("msg"), "rDebug")
        .def("get_member_schedule", &ScriptAPI::getMemberSchedule, "获取成员的排班表（空闲状态）")
        .def("find_member", &ScriptAPI::findMemberByName,  py::arg("name"), py::arg("index") = 0, "根据姓名查找成员ID。index代表第几个（0代表第一个，1代表第二个）")
        .def("set_member_property", &ScriptAPI::setMemberPropertyToModel);
}

