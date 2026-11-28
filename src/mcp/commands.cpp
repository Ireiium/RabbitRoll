#include "commands.h"
#include "commandcontext.h"
#include "ipcprotocol.h"
#include "componentmanager.h"
#include "projectmanager.h"
#include "projectsetting.h"
#include "backendworker.h"
#include "projectworker.h"
#include "projectactioncontroller.h"
#include <QTemporaryFile>
#include <QSaveFile>
#include <qt_windows.h>
#include "scriptengine.h"
#include "settings.h"
#include "rschfile.h"
#ifdef ENABLE_AI_SUPPORT
#include "AIServiceInterface.h"
#endif
#include "undocommands.h"
#include <QtConcurrent>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUndoCommand>

using namespace RabbitRollBusiness;
namespace {
using namespace RabbitRollMcp;
struct Range { bool enabled; QDate start, end; int tolerance; };
Range range() {
    auto* s = ProjectSetting::instance();
    return {s->_enableSerchDateRange,s->_searchStartDate,s->_searchEndDate,s->_toleranceValue};
}
class RangeCommand : public QUndoCommand {
    Range before, after;
    void apply(const Range& r) {
        auto* s = ProjectSetting::instance();
        s->setSerachDateRange(r.start,r.end);
        s->enableSerachDateRange(r.enabled);
        s->setToleranceValue(r.tolerance);
    }
public:
    explicit RangeCommand(Range value) : before(range()), after(value) { setText("设置排班搜索范围"); }
    void redo() override { apply(after); }
    void undo() override { apply(before); }
};
class ModeCommand : public QUndoCommand {
    EditorDutyModel::ColumnMode before, after;
public:
    explicit ModeCommand(int mode) : before(editorModel()->getColumnMode()), after(EditorDutyModel::ColumnMode(mode)) { setText("切换排班模式"); }
    void redo() override { editorModel()->setColumnMode(after); }
    void undo() override { editorModel()->setColumnMode(before); }
};
// A script may update member fields as well as assignments. Keep the combined
// operation in one editor undo entry so undo never leaves half a script applied.
class ApplyScheduleCommand : public QUndoCommand {
    QHash<QString,QList<QUuid>> before, after;
    QMap<QUuid,QVariantMap> oldProperties, properties;
    EditorDutyModel::ColumnMode mode;
    void apply(const QHash<QString,QList<QUuid>>& assignments, const QMap<QUuid,QVariantMap>& updates) {
        for (auto it = updates.cbegin(); it != updates.cend(); ++it)
            for (auto p = it.value().cbegin(); p != it.value().cend(); ++p)
                memberController()->updateMemberPropertyDirectly(memberModel()->getRowById(it.key()),p.key(),p.value());
        editorModel()->setColumnMode(mode);
        editorModel()->setSelectedIdsMap(assignments);
        editorModel()->refreshView();
    }
public:
    ApplyScheduleCommand(const QHash<QString,QList<QUuid>>& result, const QMap<QUuid,QVariantMap>& updates)
        : after(result), properties(updates), mode(editorModel()->getColumnMode()) {
        const auto& all = editorModel()->getSelectedIdsMap();
        for (auto it = all.cbegin(); it != all.cend(); ++it)
            if (it.key().startsWith(QString::number(mode) + '_')) before.insert(it.key(),it.value());
        for (auto it = updates.cbegin(); it != updates.cend(); ++it) {
            const auto* data = memberModel()->getMemberDataPtr(memberModel()->getRowById(it.key()));
            for (auto p = it.value().cbegin(); p != it.value().cend(); ++p) oldProperties[it.key()][p.key()] = data->properties.value(p.key());
        }
        setText("MCP 排班");
    }
    void redo() override { apply(after,properties); }
    void undo() override { apply(before,oldProperties); }
};
struct Prepared {
    QList<RschImport::Row> imports;
    QHash<QString,QList<QUuid>> schedule;
    QMap<QUuid,QVariantMap> properties;
    QJsonObject error;
};
}

RabbitRollCommands::RabbitRollCommands(QObject* parent) : QObject(parent) {}
RabbitRollCommands::~RabbitRollCommands() { cancelPending(); }
bool RabbitRollCommands::handles(const QString& op) {
    return QStringList{"list_members","get_schedule","list_scripts","get_job_status","update_member_properties",
        "manage_properties","configure_schedule","assign_members","import_rsch","auto_schedule","run_schedule_script","export_excel","export_rmem","save_project","new_project","open_project","close_project","manage_members",
        "get_member_schedule","edit_member_schedule","import_rmem","export_rsch","undo","redo","cancel_job",
        "preview_schedule","get_schedule_draft","apply_schedule_draft","read_script","write_script","delete_script","ai_schedule"}.contains(op);
}
void RabbitRollCommands::cancelPending() {
    for (const auto& job : jobs) {
        if (job->state != "running") continue;
        job->cancelled.store(true);
        job->state = "cancelled";
        job->result = failure("cancelled","MCP 已停用，任务结果不会应用。");
    }
    runningJob.clear();
}
QJsonObject RabbitRollCommands::execute(const QString& op, const QJsonObject& args) {
    if (op == "cancel_job") {
        if (!keys(args,{"job_id"}) || !args["job_id"].isString() || !jobs.contains(args["job_id"].toString())) return failure("job_not_found","任务不存在。");
        auto job = jobs[args["job_id"].toString()];
        if (job->state == "running") { job->cancelled.store(true); job->state="cancelled"; job->result=failure("cancelled","任务已取消，结果不会应用。"); if(runningJob==job->id)runningJob.clear(); }
        return QJsonObject{{"ok",true},{"data",QJsonObject{{"job_id",job->id},{"state",job->state}}}};
    }
    if (op == "get_schedule_draft") {
        if(!keys(args,{"draft_id"}) || !drafts.contains(args["draft_id"].toString()))return failure("draft_not_found","草案不存在。");
        const auto draft = drafts[args["draft_id"].toString()]; QJsonObject cells,props;
        for(auto it=draft.schedule.cbegin();it!=draft.schedule.cend();++it){QJsonArray ids;for(const auto& id:it.value())ids.append(id.toString(QUuid::WithoutBraces));cells[it.key()]=ids;}
        for(auto it=draft.properties.cbegin();it!=draft.properties.cend();++it)props[it.key().toString(QUuid::WithoutBraces)]=QJsonObject::fromVariantMap(it.value());
        return success({{"draft_id",args["draft_id"]},{"based_on",draft.guard},{"cells",cells},{"properties",props}});
    }
    if (op == "get_member_schedule") {
        const QUuid id(args["member_id"].toString());
        if(!keys(args,{"member_id","project_id"})||!args["member_id"].isString()||(args.contains("project_id")&&!args["project_id"].isString()))return failure("invalid_arguments","需要成员UUID和可选项目ID。");
        if(ProjectManager::instance()->isLoading())return failure("project_loading","正在切换项目。");
        if(!ProjectManager::instance()->hasProject())return failure("no_project","请先打开项目。");
        if(!args["project_id"].toString().isEmpty()&&args["project_id"]!=ProjectManager::instance()->projectSessionId())return failure("project_changed","项目已切换。");
        const int row=memberModel()->getRowById(id); if(row<0||id.isNull())return failure("member_not_found","成员不存在。");
        const auto section=ProjectManager::instance()->memberExportSnapshot()["memberData"].toObject(); return success(section["list"].toArray()[row].toObject());
    }
    if (op == "read_script") {
        if(!keys(args,{"name"}))return failure("invalid_arguments","需要脚本 name。");
        QFile file(scriptPath(args["name"].toString()));
        if(file.fileName().isEmpty()||!file.open(QIODevice::ReadOnly)||file.size()>256*1024)return failure("script_not_found","脚本不存在、无法读取或超过256KiB。");
        const auto data=file.readAll();return QJsonObject{{"ok",true},{"data",QJsonObject{{"name",args["name"]},{"code",QString::fromUtf8(data)},{"sha256",QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex())}}}};
    }
    if (op == "get_job_status") {
        if (!keys(args,{"job_id"}) || !args["job_id"].isString() || !jobs.contains(args["job_id"].toString()))
            return failure("job_not_found","任务不存在或已过期。");
        const auto job = jobs.value(args["job_id"].toString());
        return QJsonObject{{"ok",true},{"data",QJsonObject{{"job_id",job->id},{"operation",job->operation},
            {"state",job->state},{"result",job->result}}}};
    }
    if (op == "list_scripts") {
        if (!args.isEmpty()) return failure("invalid_arguments","此工具无参数。");
        QJsonArray names;
        const QDir dir(QCoreApplication::applicationDirPath() + "/scripts");
        for (const auto& file : dir.entryList({"*.py"},QDir::Files | QDir::NoSymLinks,QDir::Name))
            if (!scriptPath(file).isEmpty()) names.append(file);
        return QJsonObject{{"ok",true},{"data",QJsonObject{{"scripts",names}}}};
    }
    if (op == "list_members" || op == "get_schedule") {
        auto* p = ProjectManager::instance();
        if (!keys(args,{"project_id"}) || (args.contains("project_id") && !args["project_id"].isString())) return failure("invalid_arguments","project_id 必须为字符串。");
        if (p->isLoading()) return failure("project_loading","正在切换项目。");
        if (!p->hasProject()) return failure("no_project","请先打开或新建项目。");
        if (!args["project_id"].toString().isEmpty() && args["project_id"].toString() != p->projectSessionId()) return failure("project_changed","项目已切换。");
        QJsonArray result;
        if (op == "list_members") {
            for (const auto& member : memberController()->importSnapshot()) result.append(QJsonObject{
                {"id",member.id.toString(QUuid::WithoutBraces)},{"properties",QJsonObject::fromVariantMap(member.properties)},
                {"enabled",member.enableScheduling},{"note",member.note}});
            return success({{"members",result},{"properties",QJsonArray::fromStringList(memberController()->getAllPropertyNames())}});
        }
        auto* model = editorModel();
        for (int row = 0; row < model->rowCount(); ++row) for (int col = 0; col < model->columnCount(); ++col) {
            QJsonArray ids;
            for (const auto& id : model->getSelectedIds(row,col)) ids.append(id.toString(QUuid::WithoutBraces));
            result.append(QJsonObject{{"target",model->headerData(col,Qt::Horizontal).toString()},
                {"time_range",model->headerData(row,Qt::Vertical).toString()},{"member_ids",ids}});
        }
        return success({{"cells",result},{"mode",model->getColumnMode() == 1 ? "date" : "weekday"}});
    }
    const auto requestId = args["request_id"].toString();
    if (requestId.isEmpty() || requestId.size() > 128) return failure("invalid_arguments","写操作需要唯一 request_id（不超过 128 字符），重试时使用相同请求。");
    const auto digest = QCryptographicHash::hash(op.toUtf8() + QJsonDocument(args).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);
    if (replies.contains(requestId)) {
        const auto saved = replies.value(requestId);
        return saved.first == digest ? saved.second : failure("request_id_conflict","该 request_id 已用于不同请求。");
    }
    if (!runningJob.isEmpty()) return failure("busy","已有 MCP 任务正在执行，请查询任务状态。");
    const auto check = precondition(args,QStringList{"new_project","open_project","write_script","delete_script"}.contains(op));
    if (!check.isEmpty()) return check;
    QJsonObject result;
    if (op == "export_excel" || op == "export_rmem" || op == "save_project" || op == "export_rsch" || op == "write_script") result = startOutputJob(op,args);
    else if (op == "open_project" || op == "import_rmem") result = startInputJob(op,args);
    else if (op == "preview_schedule") {
        auto copy=args; const auto method=copy.take("method").toString();copy["preview"]=true;
        if(!QStringList{"auto","script","ai"}.contains(method))return failure("invalid_arguments","method 为 auto、script 或 ai。");
        result=startJob(method=="auto"?"auto_schedule":method=="script"?"run_schedule_script":"ai_schedule",copy);
    }
    else if (op == "import_rsch" || op == "auto_schedule" || op == "run_schedule_script" || op == "ai_schedule") result = startJob(op,args);
    else if(QStringList{"new_project","close_project","manage_members","edit_member_schedule","undo","redo","apply_schedule_draft","delete_script"}.contains(op)) result=extended(op,args);
    else result = applyCommand(op,args);
    if (result["ok"].toBool()) {
        replies.insert(requestId,{digest,result});
        requestOrder.enqueue(requestId);
        while (requestOrder.size() > 128) replies.remove(requestOrder.dequeue());
    }
    return result;
}
QJsonObject RabbitRollCommands::applyCommand(const QString& op, const QJsonObject& args) {
    const QStringList common{"request_id","project_id","expected_revision"};
    auto* m = memberModel(); auto* mc = memberController();
    auto* e = editorModel(); auto* ec = editorController();
    if (op == "update_member_properties") {
        if (!keys(args,common + QStringList{"changes"}) || !args["changes"].isArray()
            || args["changes"].toArray().isEmpty() || args["changes"].toArray().size() > 500) return failure("invalid_arguments","changes 需要 1–500 个成员属性对象。");
        QSet<QUuid> ids;
        for (const auto& value : args["changes"].toArray()) {
            const auto item = value.toObject(); const QUuid id(item["member_id"].toString());
            if (!value.isObject() || !keys(item,{"member_id","properties"}) || id.isNull() || ids.contains(id)
                || m->getRowById(id) < 0 || !item["properties"].isObject() || item["properties"].toObject().isEmpty())
                return failure("invalid_arguments","成员 UUID 无效、重复或不存在，或 properties 为空。");
            ids.insert(id);
            const auto props = item["properties"].toObject();
            for (auto p = props.constBegin(); p != props.constEnd(); ++p)
                if (!mc->getAllPropertyNames().contains(p.key()) || !p.value().isString() || p.value().toString().size() > 4096
                    || (p.key() == "姓名" && p.value().toString().trimmed().isEmpty()))
                    return failure("invalid_arguments","只能修改已存在的属性，值必须为不超过 4096 字符的字符串，姓名不能为空。");
        }
        mc->getUndoStack()->beginMacro("MCP 批量修改成员属性");
        for (const auto& value : args["changes"].toArray()) {
            const auto item = value.toObject(); const auto props = item["properties"].toObject();
            for (auto p = props.constBegin(); p != props.constEnd(); ++p) mc->updateMemberProperty(QUuid(item["member_id"].toString()),p.key(),p.value().toString());
        }
        mc->getUndoStack()->endMacro();
        return success({{"updated_members",ids.size()}});
    }
    if (op == "manage_properties") {
        QStringList names; const auto action = args["action"].toString();
        if (!keys(args,common + QStringList{"action","names"}) || (action != "add" && action != "remove")
            || !strings(args["names"],names,100) || names.isEmpty()) return failure("invalid_arguments","action 为 add/remove；names 为不重复的属性名数组。");
        for (const auto& name : names)
            if (name.trimmed() != name || name.isEmpty() || name.size() > 8 || name == "姓名" || name == "序号" || name.startsWith('_')
                || (action == "add" ? mc->getAllPropertyNames().contains(name) : !mc->getAllPropertyNames().contains(name)))
                return failure("invalid_arguments","属性名称或状态无效；保留姓名、序号，名称最多 8 字且不能以下划线开头。");
        mc->getUndoStack()->beginMacro(action == "add" ? "MCP 添加属性" : "MCP 删除属性");
        for (const auto& name : names) {
            if (action == "add") mc->getUndoStack()->push(new MemberAddPropertyCommand(mc,name,nullptr,true));
            else mc->removeProperty(name);
        }
        mc->getUndoStack()->endMacro();
        return success();
    }
    if (op == "configure_schedule") {
        QStringList columns,times; const auto mode = args["mode"].toString();
        if (!keys(args,common + QStringList{"mode","columns","time_ranges","search_range"}) || (mode != "date" && mode != "weekday")
            || !strings(args["columns"],columns,1000) || !strings(args["time_ranges"],times,100)
            || columns.size() * times.size() > 10000) return failure("invalid_arguments","mode=date/weekday；columns、time_ranges 为无重复数组，最多 10000 个单元格。");
        for (const auto& col : columns) if (mode == "date" ? !validDate(col) : !Days.contains(col)) return failure("invalid_arguments","无效日期或星期。");
        for (const auto& time : times) if (!validTime(time)) return failure("invalid_arguments","时间必须为同日 HH:mm-HH:mm，起点早于终点。");
        auto nextRange = range();
        if (args.contains("search_range")) {
            const auto obj = args["search_range"].toObject();
            if (!args["search_range"].isObject() || !keys(obj,{"enabled","start","end","tolerance_weeks"})
                || !obj["enabled"].isBool() || !validDate(obj["start"].toString()) || !validDate(obj["end"].toString())
                || obj["start"].toString() > obj["end"].toString() || !obj["tolerance_weeks"].isDouble()
                || obj["tolerance_weeks"].toDouble() != obj["tolerance_weeks"].toInt(-1)
                || obj["tolerance_weeks"].toInt(-1) < 0 || obj["tolerance_weeks"].toInt() > 5)
                return failure("invalid_arguments","search_range 需要 enabled、有效 start/end 和 0–5 的整数 tolerance_weeks。");
            nextRange = {obj["enabled"].toBool(),QDate::fromString(obj["start"].toString(),Qt::ISODate),
                QDate::fromString(obj["end"].toString(),Qt::ISODate),obj["tolerance_weeks"].toInt()};
        }
        ec->getUndoStack()->beginMacro("MCP 设置排班表时间");
        const int targetMode = mode == "date" ? 1 : 0;
        if (int(e->getColumnMode()) != targetMode) ec->getUndoStack()->push(new ModeCommand(targetMode));
        for (int row = e->rowCount() - 1; row >= 0; --row) if (!times.contains(e->getTimesRow()->at(row))) ec->deleteTimeRow(row);
        for (const auto& time : times) if (!e->getTimesRow()->contains(time)) ec->getUndoStack()->push(new EditorDutyAddTimeCommand(ec,time));
        for (int col = e->columnCount() - 1; col >= 0; --col)
            if (!columns.contains(e->headerData(col,Qt::Horizontal).toString())) ec->deleteColumn(col);
        auto current = targetMode == 1 ? e->getDateColumns() : e->getWeekColumns();
        for (const auto& col : columns) if (!current.contains(col)) ec->getUndoStack()->push(new EditorDutyAddColumnCommand(ec,targetMode,col));
        if (args.contains("search_range")) ec->getUndoStack()->push(new RangeCommand(nextRange));
        ec->getUndoStack()->endMacro();
        return success();
    }
    if (op == "assign_members") {
        if (!keys(args,common + QStringList{"cells"}) || !args["cells"].isArray() || args["cells"].toArray().isEmpty()
            || args["cells"].toArray().size() > 1000) return failure("invalid_arguments","cells 需要 1–1000 个单元格对象。");
        const auto columns = e->getColumnMode() == 1 ? e->getDateColumns() : e->getWeekColumns();
        QSet<QString> cellKeys;
        for (const auto& value : args["cells"].toArray()) {
            const auto cell = value.toObject(); QStringList ids;
            const auto target = cell["target"].toString(), time = cell["time_range"].toString();
            const QString key = QString::number(e->getColumnMode()) + '_' + time + '_' + target;
            if (!value.isObject() || !keys(cell,{"target","time_range","member_ids"}) || !columns.contains(target)
                || !e->getTimesRow()->contains(time) || cellKeys.contains(key) || !strings(cell["member_ids"],ids,500))
                return failure("invalid_arguments","无效或重复的排班位置、成员列表。");
            cellKeys.insert(key);
            const auto candidates = IdleMatchService::instance()->getAvailableMembers(e->getColumnMode(),target,time);
            QSet<QUuid> seen;
            for (const auto& text : ids) {
                const QUuid id(text);
                if (id.isNull() || seen.contains(id) || !candidates.contains(id.toString()))
                    return failure("member_unavailable","所选成员不存在、不参与排班或未通过当前空闲规则。");
                seen.insert(id);
            }
        }
        ec->getUndoStack()->beginMacro("MCP 安排成员");
        for (const auto& value : args["cells"].toArray()) {
            const auto cell = value.toObject(); QList<QUuid> ids;
            for (const auto& id : cell["member_ids"].toArray()) ids.append(QUuid(id.toString()));
            ec->setSelectedIds(e->index(e->getTimesRow()->indexOf(cell["time_range"].toString()),columns.indexOf(cell["target"].toString())),ids);
        }
        ec->getUndoStack()->endMacro();
        return success();
    }
    return failure("unknown_operation","不支持的业务操作。");
}
QJsonObject RabbitRollCommands::startJob(const QString& op, const QJsonObject& args) {
    const QStringList common{"request_id","project_id","expected_revision"};
    QStringList paths;
    RschImport::Options options;
    const int targetCount = args["target_per_cell"].toInt(-1);
    QString script;
    if (op == "import_rsch") {
        if (!keys(args,common + QStringList{"paths","format","semester_start","existing_only","replace_schedule","overwrite_properties","targets"})
            || !strings(args["paths"],paths,500) || paths.isEmpty()) return failure("invalid_arguments","paths 需要 1–500 个不重复的 .rsch 文件绝对路径。");
        for (auto& path : paths) {
            const QFileInfo file(path);
            if (!file.isAbsolute() || !file.isFile() || file.suffix().compare("rsch",Qt::CaseInsensitive) != 0)
                return failure("invalid_arguments","仅接受存在的 .rsch 文件绝对路径。");
            path = QDir::cleanPath(file.absoluteFilePath());
        }
        if (args.contains("format") && !args["format"].isString()) return failure("invalid_arguments","format 必须为字符串。");
        options.format = args["format"].toString("姓名");
        if (args.contains("semester_start") && !validDate(args["semester_start"].toString())) return failure("invalid_arguments","semester_start 需要 yyyy-MM-dd。");
        options.semesterStart = QDate::fromString(args["semester_start"].toString(),Qt::ISODate);
        for (const auto& field : {"existing_only","replace_schedule","overwrite_properties"})
            if (args.contains(field) && !args[field].isBool()) return failure("invalid_arguments","导入选项必须为布尔值。");
        options.existingOnly = args["existing_only"].toBool();
        options.replaceSchedule = args["replace_schedule"].toBool();
        options.overwriteProperties = args["overwrite_properties"].toBool();
        if (args.contains("targets")) {
            if (!args["targets"].isObject()) return failure("invalid_arguments","targets 为路径到成员 UUID 或 new 的对象。");
            const auto targets = args["targets"].toObject();
            for (auto it = targets.constBegin(); it != targets.constEnd(); ++it) {
                if (!paths.contains(QDir::cleanPath(it.key())) || !it.value().isString()
                    || (it.value().toString() != "new" && memberModel()->getRowById(QUuid(it.value().toString())) < 0))
                    return failure("invalid_arguments","targets 中存在无效路径或成员。");
                options.targets.insert(QDir::cleanPath(it.key()),it.value().toString());
            }
        }
    } else {
        const auto extra = op == "auto_schedule" ? QStringList{"target_per_cell","prioritize_fewer","allow_multiple","preview"} : op=="ai_schedule" ? QStringList{"target_per_cell","prompt","thinking","preview"} : QStringList{"target_per_cell","script_name","preview"};
        if (!keys(args,common + extra) || !args["target_per_cell"].isDouble() || args["target_per_cell"].toDouble() != targetCount
            || targetCount < 1 || targetCount > 500 || editorModel()->rowCount() == 0 || editorModel()->columnCount() == 0)
            return failure("invalid_arguments","需要非空排班表和 1–500 的整数 target_per_cell。");
        if(args.contains("preview")&&!args["preview"].isBool())return failure("invalid_arguments","preview 为布尔值。");
        if (op == "ai_schedule") {
#ifdef ENABLE_AI_SUPPORT
            if(!Settings::instance()->getEnableAI())return failure("ai_disabled","请在界面设置中启用内置 AI。");
            if(!AIServiceInterface::instance()->hasApiKey())return failure("ai_not_configured","请在界面配置 API key。");
            if(!args["prompt"].isString()||args["prompt"].toString().size()>16000||(args.contains("thinking")&&!args["thinking"].isBool()))return failure("invalid_arguments","需要 prompt 字符串和布尔 thinking。");
#else
            return failure("feature_disabled","此构建未包含内置 AI；其他 MCP 功能仍可用。");
#endif
        } else if (op == "auto_schedule") {
            for (const auto& field : {"prioritize_fewer","allow_multiple"})
                if (args.contains(field) && !args[field].isBool()) return failure("invalid_arguments","排班选项必须为布尔值。");
        } else {
            script = scriptPath(args["script_name"].toString());
            if (script.isEmpty()) return failure("script_not_found","只能运行 list_scripts 中已有的本地脚本名称。");
            if (!ScriptEngine::instance()->_canScriptWork) return failure("script_unavailable","现有 Python 脚本引擎尚未就绪。");
        }
    }
    while (jobs.size() >= 64) {
        auto oldest = jobs.begin();
        jobs.erase(oldest);
    }
    const auto job = std::make_shared<Job>();
    job->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    job->operation = op; job->guard = args;
    jobs.insert(job->id,job); runningJob = job->id;
    const auto members = memberController()->importSnapshot();
    const auto importRevision = memberController()->importRevision();
    auto candidates = editorModel()->getCellDataMap();
    const int mode = editorModel()->getColumnMode();
    for (auto it = candidates.begin(); it != candidates.end();) {
        if (!it.key().startsWith(QString::number(mode) + '_')) it = candidates.erase(it); else ++it;
    }
    auto* watcher = new QFutureWatcher<Prepared>(this);
    connect(watcher,&QFutureWatcher<Prepared>::finished,this,[this,watcher,job,importRevision,candidates] {
        const auto prepared = watcher->result(); watcher->deleteLater();
        if (runningJob == job->id) runningJob.clear();
        if (job->cancelled.load()) return;
        auto fail = [&](const QJsonObject& error) { job->state = "failed"; job->result = error; };
        if (!prepared.error.isEmpty()) { fail(prepared.error); return; }
        const auto check = precondition(job->guard);
        if (!check.isEmpty()) { fail(check); return; }
        if (job->operation == "import_rsch") {
            QJsonArray rows;
            for (const auto& row : prepared.imports) rows.append(QJsonObject{{"path",row.path},
                {"member_id",row.result.id.toString(QUuid::WithoutBraces)},{"created",row.create},{"warning",row.warning},{"error",row.error}});
            for (const auto& row : prepared.imports) if (!row.error.isEmpty() || !row.selected) {
                auto error = failure("import_validation_failed","RSCH 校验或成员匹配存在问题，整批未导入。"); error["rows"] = rows; fail(error); return;
            }
            QString error;
            if (!memberController()->applyRschImport(prepared.imports,importRevision,error)) { fail(failure("revision_conflict",error)); return; }
            job->result = success({{"rows",rows},{"imported_members",prepared.imports.size()}});
        } else {
            for (auto it = prepared.schedule.cbegin(); it != prepared.schedule.cend(); ++it) {
                if (!candidates.contains(it.key())) { fail(failure("invalid_schedule_result","脚本返回未知排班位置。")); return; }
                for (const auto& id : it.value()) if (memberModel()->getRowById(id) < 0) { fail(failure("invalid_schedule_result","排班结果引用失效成员。")); return; }
            }
            for (auto it = prepared.properties.cbegin(); it != prepared.properties.cend(); ++it) {
                if (memberModel()->getRowById(it.key()) < 0) { fail(failure("invalid_script_result","脚本引用失效成员。")); return; }
                for (auto p = it.value().cbegin(); p != it.value().cend(); ++p)
                    if (!memberController()->getAllPropertyNames().contains(p.key()) || (p.key() == "姓名" && p.value().toString().trimmed().isEmpty())) {
                        fail(failure("invalid_script_result","脚本只能修改已存在的属性，姓名不能为空；请先添加需要的属性。")); return;
                    }
            }
            if(job->guard["preview"].toBool()) {
                while(drafts.size()>=32)drafts.erase(drafts.begin());
                const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
                drafts.insert(id,{job->guard,prepared.schedule,prepared.properties});
                job->state="completed";job->result=success({{"draft_id",id},{"cell_count",prepared.schedule.size()}});return;
            }
            editorController()->getUndoStack()->push(new ApplyScheduleCommand(prepared.schedule,prepared.properties));
            job->result = success({{"assigned_cells",prepared.schedule.size()},{"updated_members",prepared.properties.size()}});
        }
        job->state = "completed";
    });
    watcher->setFuture(QtConcurrent::run([op,args,paths,options,members,candidates,mode,script,targetCount,job] {
        Prepared result;
        try {
            if (job->cancelled.load()) return result;
            if (op == "import_rsch") result.imports = RschImport::preview(RschImport::read(paths),members,options);
            else if (op == "auto_schedule") result.schedule = BackendWorker::calculateAutoSchedule(candidates,targetCount,
                args["prioritize_fewer"].toBool(),args["allow_multiple"].toBool(),mode,&job->cancelled);
            else if(op=="ai_schedule") {
#ifdef ENABLE_AI_SUPPORT
                QString error; result.schedule=BackendWorker::calculateAISchedule(args["prompt"].toString(),candidates,members,targetCount,args["thinking"].toBool(),&job->cancelled,error);
                if(!error.isEmpty())result.error=failure("ai_failed",error);
#endif
            } else {
                QString error;
                result.schedule = ScriptEngine::instance()->runScheduleScript(script,targetCount,candidates,members,error,&result.properties,10000,&job->cancelled);
                if (!error.isEmpty()) result.error = failure("script_failed",error);
            }
        } catch (const std::exception& e) { result.error = failure("job_failed",QString::fromUtf8(e.what())); }
        return result;
    }));
    return success({{"job_id",job->id},{"state","running"}});
}


QJsonObject RabbitRollCommands::startOutputJob(const QString& op, const QJsonObject& args) {
    const bool scriptWrite=op=="write_script";
    const QStringList outputKeys = QStringList{"project_id","expected_revision","request_id","overwrite"}
        + (scriptWrite ? QStringList{"name","code","expected_sha256"} : QStringList{"path"})
        + (op=="export_excel" ? QStringList{"scope"} : op=="export_rsch" ? QStringList{"member_id"} : QStringList{});
    if (!keys(args,outputKeys) || (args.contains("overwrite")&&!args["overwrite"].isBool())) return failure("invalid_arguments","无效输出字段。");
    auto* project=ProjectManager::instance();QString code;
    QString path;
    const bool currentSave=op=="save_project"&&!args.contains("path");
    if(scriptWrite){
        const auto name=args["name"].toString();const QDir directory(QCoreApplication::applicationDirPath()+"/scripts");
        if(name.isEmpty()||QFileInfo(name).fileName()!=name||name.contains('/')||name.contains('\\')||!name.endsWith(".py")||!args["code"].isString()||args["code"].toString().toUtf8().size()>128*1024||args.contains("path"))return failure("invalid_arguments","需要 scripts 中的 .py 文件名和最多128KiB的code，不接受任意路径。");
        code=args["code"].toString();path=directory.filePath(name);
        if(!QDir().mkpath(directory.absolutePath()))return failure("write_failed","无法创建脚本目录。");
    } else path=currentSave?project->currentFilePath():args["path"].toString();
    if(path.isEmpty())return failure("path_required","首次保存/导出需要文件绝对路径。");
    const QString suffix=scriptWrite?"py":op=="export_excel"?"xlsx":op=="export_rmem"?"rmem":op=="export_rsch"?"rsch":"rroll";
    if(!QFileInfo(path).isAbsolute())return failure("invalid_path","需要绝对路径。");
    if(QFileInfo(path).suffix().isEmpty())path+='.'+suffix;
    const QFileInfo file(QDir::cleanPath(path));static const QRegularExpression invalidName(R"([<>:"|?*])");
    if(file.suffix().compare(suffix,Qt::CaseInsensitive)!=0||file.isDir()||file.isSymLink()||invalidName.match(file.fileName()).hasMatch()||file.fileName().endsWith(' ')||!QFileInfo(file.absolutePath()).isDir())return failure("invalid_path","目标路径或文件格式无效。");
    path=file.absoluteFilePath();const bool overwrite=currentSave||args["overwrite"].toBool();
    if(file.exists()&&!overwrite)return failure("file_exists","目标已存在，覆盖需要 overwrite=true。");
    if(scriptWrite&&file.exists()) {QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>256*1024||QString::fromLatin1(QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256).toHex())!=args["expected_sha256"].toString())return failure("file_changed","覆盖脚本需提供read_script返回的最新expected_sha256。");}
    QJsonObject snapshot;QList<ExportTableData> tables;
    if(op=="export_excel"){
        const auto scope=args["scope"].toString("schedule");if(scope!="schedule"&&scope!="members"&&scope!="both")return failure("invalid_arguments","scope 为schedule/members/both。");
        tables=ProjectActionController::excelExportSnapshot(scope!="members",scope!="schedule");if(tables.size()!=(scope=="both"?2:1))return failure("nothing_to_export","表格为空。");
    } else if(op=="export_rsch") {
        const int row=memberModel()->getRowById(QUuid(args["member_id"].toString()));if(row<0)return failure("member_not_found","导出RSCH需要成员UUID，内容为该成员个人时间表。");
        const auto member=project->memberExportSnapshot()["memberData"].toObject()["list"].toArray()[row].toObject();
        snapshot=QJsonObject{{"app","rabbitroll"},{"dataType","schedule"},{"version","1.0"},{"scheduleData",member["scheduleMap"]}};
        if(!RschFile::validate(snapshot).isEmpty())return failure("invalid_schedule","个人时间表存在无效数据。");
    } else if(!scriptWrite){snapshot=op=="export_rmem"?project->memberExportSnapshot():project->projectSnapshot();if(op=="save_project")snapshot["projectName"]=QFileInfo(path).completeBaseName();}
    // The staging file is in the target directory. Publication is one rename,
    // and never falls back to copying over an existing destination.
    struct StagedOutput {
        QString path;
        QJsonObject error;
        ~StagedOutput() { if (!path.isEmpty()) QFile::remove(path); }
    };
    while (jobs.size() >= 64) jobs.erase(jobs.begin());
    const auto job = std::make_shared<Job>();
    job->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    job->operation = op; job->guard = args;
    jobs.insert(job->id,job); runningJob = job->id;
    auto* watcher = new QFutureWatcher<std::shared_ptr<StagedOutput>>(this);
    connect(watcher,&QFutureWatcher<std::shared_ptr<StagedOutput>>::finished,this,[this,watcher,job,path,overwrite] {
        const auto staged = watcher->result(); watcher->deleteLater();
        if (runningJob == job->id) runningJob.clear();
        if (job->cancelled.load()) return;
        auto fail = [&](const QJsonObject& error) { job->state = "failed"; job->result = error; };
        if (!staged->error.isEmpty()) { fail(staged->error); return; }
        const auto check = precondition(job->guard,job->operation=="write_script");
        if (!check.isEmpty()) { fail(check); return; }
        if(job->operation=="write_script" && QFileInfo::exists(path)) {
            QFile current(path);if(!current.open(QIODevice::ReadOnly)||QString::fromLatin1(QCryptographicHash::hash(current.readAll(),QCryptographicHash::Sha256).toHex())!=job->guard["expected_sha256"].toString()){fail(failure("file_changed","脚本已被修改，未覆盖。"));return;}
        }
        const DWORD flags = MOVEFILE_WRITE_THROUGH | (overwrite ? MOVEFILE_REPLACE_EXISTING : 0);
        if (!MoveFileExW(reinterpret_cast<LPCWSTR>(staged->path.utf16()),reinterpret_cast<LPCWSTR>(path.utf16()),flags)) {
            const DWORD code = GetLastError();
            fail(failure(code == ERROR_ALREADY_EXISTS || code == ERROR_FILE_EXISTS ? "file_exists" : "publish_failed",
                QString("未能提交输出文件（Windows 错误 %1），原目标未被写入半成品。").arg(code)));
            return;
        }
        staged->path.clear();
        if (job->operation == "save_project") {
            ProjectManager::instance()->acceptSavedSnapshot(path,job->guard["project_id"].toString(),
                job->guard["expected_revision"].toString().toULongLong());
        }
        if(job->operation=="write_script")Q_EMIT ScriptEngine::instance()->scriptsChanged();
        job->state = "completed";
        job->result = success({{"path",path},{"snapshot_revision",job->guard["expected_revision"]}});
    });
    watcher->setFuture(QtConcurrent::run([job,path,tables,snapshot,op,code] {
        const auto result = std::make_shared<StagedOutput>();
        if (job->cancelled.load()) return result;
        {
            QTemporaryFile temporary(QFileInfo(path).absolutePath() + "/." + QFileInfo(path).fileName() + ".RabbitRoll-XXXXXX");
            if (!temporary.open()) { result->error = failure("write_failed","无法在目标目录创建临时文件：" + temporary.errorString()); return result; }
            result->path = temporary.fileName();
            temporary.setAutoRemove(false);
        } // QTemporaryFile::close() alone retains its Windows file handle.

        try {
            QString error;
            if(op=="write_script") {
                error=ScriptEngine::instance()->checkSyntax(code);
                if(error.isEmpty()){QSaveFile f(result->path);const auto data=code.toUtf8();if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit())error="写入脚本失败。";}
            } else if(op=="export_rsch") {const auto written=RschFile::write(result->path,QJsonDocument(snapshot).toJson());if(!written)error=written.message;}
            else if (op == "export_excel") BackendWorker::writeExcelFile(result->path,tables,error);
            else error = ProjectWorker::writeJsonFile(result->path,snapshot);
            if (!error.isEmpty()) result->error = failure("write_failed",error);
        } catch (const std::exception& e) { result->error = failure("write_failed",QString::fromUtf8(e.what())); }
        return result;
    }));
    return success({{"job_id",job->id},{"state","running"},{"path",path}});
}

QJsonObject RabbitRollCommands::commitSchedule(const QHash<QString,QList<QUuid>>& result,const QMap<QUuid,QVariantMap>& properties) {
    const auto candidates=editorModel()->getCellDataMap();
    for(auto it=result.cbegin();it!=result.cend();++it){if(!candidates.contains(it.key()))return failure("invalid_schedule_result","无效排班位置。");for(const auto& id:it.value())if(memberModel()->getRowById(id)<0)return failure("member_not_found","成员已失效。");}
    editorController()->getUndoStack()->push(new ApplyScheduleCommand(result,properties)); return success();
}
