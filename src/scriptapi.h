#pragma once
#include <string>
#include <vector>
#include <map>

//暴露给python的api
class ScriptAPI {
public:
    ScriptAPI() = default;
    ~ScriptAPI() = default;

    void setAvailableData(const std::map<std::string, std::vector<std::string>>& candidates);
    void setAllMembers(const std::vector<std::string>& members);
    void setMemberProperties(const std::map<std::string, std::map<std::string, std::string>>& props);
    void setMemberDutyCounts(const std::map<std::string, int>& counts);
    void setMemberSchedules(const std::map<std::string, std::map<std::string, std::map<std::string, bool>>>& schedules);
    

    //获取程序预筛选的候选人名单 (格式: { 任务槽Key : [成员UUID列表] })
    std::map<std::string, std::vector<std::string>> getTaskCandidates() const;

    //获取所有成员的 UUID 列表 (方便用户忽视预筛名单，完全自定义排班)
    std::vector<std::string> getAllMembers() const;

    //获取指定成员的自定义属性字典 (如 姓名、性别、职位等)
    std::map<std::string, std::string> getMemberProperties(const std::string& memberId) const;

    //获取指定成员已排班的次数
    int getMemberDutyCount(const std::string& memberId) const;
    std::map<std::string, int> getMemberDutyCounts() const;

    //获取指定成员的时间表 (格式: { 日期 : { 时间段 : bool(是否空闲) } })
    std::map<std::string, std::map<std::string, bool>> getMemberSchedule(const std::string& memberId) const;

    //获取成员的Quuid编号
    std::string findMemberByName(const std::string& name, int index = 0) const;

    void debug(const std::string& msg) const;

    void setMemberPropertyToModel(const std::string& memberId, const std::string& key, const std::string& value);




    bool hasModified() const { return _isModified; }
    void resetModified() { _isModified = false; _pendingProperties.clear(); }
    const std::map<std::string, std::map<std::string, std::string>>& pendingProperties() const { return _pendingProperties; }


private:
    std::map<std::string, std::vector<std::string>> _taskCandidates;
    std::vector<std::string> _allMembers;
    std::map<std::string, std::map<std::string, std::string>> _memberProperties;
    std::map<std::string, int> _memberDutyCounts;
    std::map<std::string, std::map<std::string, std::map<std::string, bool>>> _memberSchedules;

    bool _isModified = false;
    std::map<std::string, std::map<std::string, std::string>> _pendingProperties;
};
