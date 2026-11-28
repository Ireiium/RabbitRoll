#include "businessadapter.h"
#include "commands.h"
#include "commandcontext.h"
#include "ipcprotocol.h"
#include "componentmanager.h"
#include "projectmanager.h"
#include "projectsetting.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QSet>
#include <QThread>

namespace {
QJsonObject position(int row, int column, const QString& key, const QString& target, const QString& time) {
    return {{"row", row}, {"column", column}, {"cell_key", key}, {"target", target}, {"time_range", time}};
}
}

RabbitRollBusinessAdapter::RabbitRollBusinessAdapter(QObject* parent) : QObject(parent), commands(std::make_unique<RabbitRollCommands>()) {}
RabbitRollBusinessAdapter::~RabbitRollBusinessAdapter() = default;
void RabbitRollBusinessAdapter::cancelPending() { commands->cancelPending(); }
QJsonObject RabbitRollBusinessAdapter::execute(const QString& operation, const QJsonObject& arguments) {
    using namespace RabbitRollMcp;
    using namespace RabbitRollBusiness;
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    if (RabbitRollCommands::handles(operation)) return commands->execute(operation,arguments);
    QStringList allowed;
    if (operation == "query_availability") allowed = {"mode", "target", "time_range", "member_id", "project_id"};
    else if (operation == "validate_schedule") allowed = {"project_id"};
    else if (operation != "get_project_context") return failure("unknown_operation", "不支持的业务操作。");
    for (auto it = arguments.constBegin(); it != arguments.constEnd(); ++it) {
        if (!allowed.contains(it.key()) || !it.value().isString())
            return failure("invalid_arguments", "参数必须为工具定义中的字符串字段。");
    }
    auto* project = ProjectManager::instance();
    if (project->isLoading()) return failure("project_loading", "正在切换项目，请完成后重试。");
    if (!project->hasProject()) {
        auto result=failure("no_project", "请在 RabbitRoll 中新建或打开项目。");result["project_id"]="";result["revision"]=QString::number(project->changeRevision());return result;
    }
    const auto expected = arguments["project_id"].toString();
    if (!expected.isEmpty() && expected != project->projectSessionId())
        return failure("project_changed", "当前项目已切换，请重新读取项目概况。");
    auto* components = ComponentManager::instance();
    auto* members = components->getComponent<MemberInfoComponent>()->getModel();
    auto* controller = components->getComponent<MemberInfoComponent>()->getController();
    auto* schedule = components->getComponent<EditorDutyComponent>()->getModel();
    auto* settings = ProjectSetting::instance();
    auto* idle = IdleMatchService::instance();
    const int mode = schedule->getColumnMode();
    const auto columns = mode == 1 ? schedule->getDateColumns() : schedule->getWeekColumns();
    const auto times = *schedule->getTimesRow();
    const QDate start = settings->_enableSerchDateRange ? settings->_searchStartDate : QDate::currentDate();
    const QDate end = settings->_enableSerchDateRange ? settings->_searchEndDate : start.addMonths(3);
    if (operation == "get_project_context") {
        int enabled = 0;
        for (int row = 0; row < members->rowCount(); ++row)
            if (members->getMemberDataPtr(row)->enableScheduling) ++enabled;
        auto* editorController = components->getComponent<EditorDutyComponent>()->getController();
        auto history = [](QUndoStack* stack) { return QJsonObject{{"can_undo",stack->canUndo()},{"can_redo",stack->canRedo()},
            {"undo_text",stack->undoText()},{"redo_text",stack->redoText()}}; };
        return success({{"history",QJsonObject{{"members",history(controller->getUndoStack())},{"schedule",history(editorController->getUndoStack())}}},
            {"name", settings->_projectName}, {"file_path", project->currentFilePath()},
            {"modified", project->isDirty()}, {"mode", mode == 1 ? "date" : "weekday"},
            {"member_count", members->rowCount()}, {"enabled_member_count", enabled},
            {"columns", QJsonArray::fromStringList(columns)}, {"time_ranges", QJsonArray::fromStringList(times)},
            {"availability_calculating", idle->isCalculating()},
            {"search_range", QJsonObject{{"start", start.toString(Qt::ISODate)}, {"end", end.toString(Qt::ISODate)},
                {"explicit", settings->_enableSerchDateRange}, {"tolerance_weeks", settings->_toleranceValue}}}});
    }
    if (operation == "query_availability") {
        const auto requestedMode = arguments["mode"].toString();
        const auto target = arguments["target"].toString();
        const auto time = arguments["time_range"].toString();
        const auto id = arguments["member_id"].toString();
        if ((requestedMode != "date" && requestedMode != "weekday")
            || !validTime(time) || (requestedMode == "date" ? !validDate(target) : !Days.contains(target))
            || (!id.isEmpty() && QUuid(id).isNull()))
            return failure("invalid_arguments", "需要 mode=date/weekday、有效 target 和 HH:mm-HH:mm；member_id 必须为 UUID。");
        if ((requestedMode == "date" ? 1 : 0) != mode)
            return failure("mode_mismatch", "查询模式与当前项目排班模式不同。");
        if (!id.isEmpty() && members->getRowById(QUuid(id)) < 0)
            return failure("member_not_found", "指定成员不存在。");
        if (idle->isCalculating()) return failure("calculating", "空闲数据正在更新，请稍后重试。");
        if (!start.isValid() || !end.isValid() || start > end)
            return failure("invalid_project_range", "项目搜索日期范围无效。");
        if (mode == 1 && settings->_enableSerchDateRange
            && (QDate::fromString(target, Qt::ISODate) < start || QDate::fromString(target, Qt::ISODate) > end))
            return failure("outside_search_range", "日期不在项目配置的搜索范围内。");
        const auto snapshot = controller->getMemberScheduleSnapshot();
        const auto mask = IdleMatchService::timeStringToMask(time);
        QJsonArray result;
        for (const auto& member : snapshot) {
            if ((!id.isEmpty() && QUuid(id) != member.id)
                || !IdleMatchService::matchesAvailability(member, mode, target, mask, start, end,
                    settings->_enableSerchDateRange, settings->_toleranceValue)) continue;
            result.append(QJsonObject{{"id", member.id.toString(QUuid::WithoutBraces)}, {"name", member.name}});
        }
        return success({{"members", result}, {"count", result.size()}, {"target", target}, {"time_range", time}});
    }
    if (idle->isCalculating()) return failure("calculating", "空闲数据正在更新，请稍后重试。");
    if (!start.isValid() || !end.isValid() || start > end)
        return failure("invalid_project_range", "项目搜索日期范围无效。");
    const auto snapshot = controller->getMemberScheduleSnapshot();
    QHash<QUuid, int> snapshotRows;
    for (int i = 0; i < snapshot.size(); ++i) snapshotRows.insert(snapshot[i].id, i);
    QJsonArray issues;
    bool truncated = false;
    auto report = [&](const QString& code, const QString& reason, const QJsonObject& pos,
                      const QUuid& id = {}, const QJsonObject& related = {}) {
        if (issues.size() >= 1000) { truncated = true; return; }
        QJsonObject issue{{"code", code}, {"reason", reason}, {"position", pos}};
        if (!id.isNull()) {
            issue["member_id"] = id.toString(QUuid::WithoutBraces);
            issue["member_name"] = members->getNameById(id);
        }
        if (!related.isEmpty()) issue["related_position"] = related;
        issues.append(issue);
    };
    struct Assignment { QUuid id; QString target; TimeMask mask; QJsonObject position; };
    QHash<QString, QList<Assignment>> previous;
    QStringList keys = schedule->getSelectedIdsMap().keys();
    keys.sort();
    int checked = 0;
    for (const auto& key : keys) {
        if (truncated) break;
        const auto ids = schedule->getSelectedIdsMap().value(key);
        if (ids.isEmpty()) continue;
        const auto parts = key.split('_');
        // Both modes are retained by the editor; only the active one is checked.
        if (parts.size() == 3 && (parts[0] == "0" || parts[0] == "1") && parts[0].toInt() != mode) continue;
        const QString time = parts.value(1), target = parts.value(2);
        const int row = times.indexOf(time), col = columns.indexOf(target);
        const auto pos = position(row, col, key, target, time);
        if (parts.size() != 3 || parts[0] != QString::number(mode) || row < 0 || col < 0
            || !validTime(time) || (mode == 1 ? !validDate(target) : !Days.contains(target))) {
            report("invalid_position", "分配记录的位置、日期或时间段无效（行列编号从 0 开始）。", pos);
            continue;
        }
        ++checked;
        const auto mask = IdleMatchService::timeStringToMask(time);
        QSet<QUuid> unique;
        for (const auto& id : ids) {
            if (unique.contains(id)) { report("duplicate_assignment", "同一单元格重复分配了此成员。", pos, id); continue; }
            unique.insert(id);
            const int memberRow = members->getRowById(id);
            if (memberRow < 0) { report("missing_member", "分配引用的成员已不存在。", pos, id); continue; }
            const auto* data = members->getMemberDataPtr(memberRow);
            if (!data->enableScheduling) report("member_disabled", "此成员已关闭参与排班。", pos, id);
            else if (!IdleMatchService::matchesAvailability(snapshot[snapshotRows.value(id)], mode, target, mask,
                start, end, settings->_enableSerchDateRange, settings->_toleranceValue)) {
                const bool outOfRange = mode == 1 && settings->_enableSerchDateRange &&
                    (QDate::fromString(target, Qt::ISODate) < start || QDate::fromString(target, Qt::ISODate) > end);
                report(outOfRange ? "outside_search_range" : "unavailable_member",
                    "此分配未通过当前项目的日期范围、个人繁忙时间及容忍周数规则。", pos, id);
            }
            const auto group = id.toString() + '_' + target;
            for (const auto& earlier : previous.value(group)) {
                if ((earlier.mask & mask).any()) report("overlapping_assignments", "同一成员被分配到时间重叠的班次。", pos, id, earlier.position);
                if (truncated) break;
            }
            previous[group].append({id, target, mask, pos});
        }
    }
    return success({{"issues", issues}, {"issue_count", issues.size()}, {"truncated", truncated},
        {"checked_cells", checked}, {"valid", issues.isEmpty() && !truncated},
        {"scope", "仅当前模式；不推断人数、公平性或未配置的其他规则。行列编号从 0 开始。"}});
}
