#include "commands.h"
#include "commandcontext.h"
#include "undocommands.h"
#include "scriptengine.h"
#include "rschfile.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QUndoCommand>
#include <algorithm>
using namespace RabbitRollBusiness;
using namespace RabbitRollMcp;
namespace {
const QStringList Common{"project_id","expected_revision","request_id"};
QHash<QString,QList<QUuid>> filtered(const QHash<QString,QList<QUuid>>& source, const QJsonObject& section) {
    QSet<QUuid> valid;
    for (const auto& item : section["list"].toArray()) {
        const auto obj = item.toObject();
        if (obj["enableScheduling"].toBool(true)) valid.insert(QUuid(obj["id"].toString()));
    }
    auto result = source;
    for (auto it = result.begin(); it != result.end(); ++it) {
        auto& ids = it.value();
        ids.erase(std::remove_if(ids.begin(),ids.end(),[&](const QUuid& id){return !valid.contains(id);}),ids.end());
    }
    return result;
}
// Apply only fields changed by this member operation. A script on the
// separate schedule history may have edited a different field since then.
QJsonObject patchObject(QJsonObject current, const QJsonObject& from, const QJsonObject& to) {
    QSet<QString> names;
    for(const auto& key:from.keys()) names.insert(key);
    for(const auto& key:to.keys()) names.insert(key);
    for(const auto& key:names) {
        if(from[key]==to[key]) continue;
        if(!to.contains(key)) current.remove(key);
        else if(from[key].isObject() && to[key].isObject()) current[key]=patchObject(current[key].toObject(),from[key].toObject(),to[key].toObject());
        else current[key]=to[key];
    }
    return current;
}
QJsonObject patchMembers(const QJsonObject& from, const QJsonObject& to) {
    auto current=ProjectManager::instance()->memberExportSnapshot()["memberData"].toObject();
    auto currentList=current["list"].toArray();
    auto oldSection=from,newSection=to;oldSection.remove("list");newSection.remove("list");
    current=patchObject(current,oldSection,newSection);
    QMap<QUuid,QJsonObject> oldMembers,newMembers;
    for(const auto& item:from["list"].toArray()) { const auto member=item.toObject();oldMembers[QUuid(member["id"].toString())]=member; }
    for(const auto& item:to["list"].toArray()) { const auto member=item.toObject();newMembers[QUuid(member["id"].toString())]=member; }
    for(int i=currentList.size()-1;i>=0;--i) {
        const auto id=QUuid(currentList[i].toObject()["id"].toString());
        if(oldMembers.contains(id) && !newMembers.contains(id))currentList.removeAt(i);
    }
    int position=0;
    for(const auto& item:to["list"].toArray()) {
        const auto member=item.toObject();const auto id=QUuid(member["id"].toString());int row=-1;
        for(int i=0;i<currentList.size();++i)if(QUuid(currentList[i].toObject()["id"].toString())==id){row=i;break;}
        if(row<0)currentList.insert(qMin(position,int(currentList.size())),member);
        else currentList[row]=patchObject(currentList[row].toObject(),oldMembers.value(id),member);
        ++position;
    }
    current["list"]=currentList;
    return current;
}
class MemberStateCommand : public QUndoCommand {
    QJsonObject before, after;
    QHash<QString,QList<QUuid>> beforeAssignments, afterAssignments;
public:
    MemberStateCommand(const QJsonObject& section, const QString& text) :
        before(ProjectManager::instance()->memberExportSnapshot()["memberData"].toObject()), after(section),
        beforeAssignments(editorModel()->getSelectedIdsMap()), afterAssignments(filtered(beforeAssignments,section)) { setText(text); }
    void redo() override {
        const auto section=patchMembers(before,after);
        ProjectManager::instance()->restoreMemberState(section,filtered(editorModel()->getSelectedIdsMap(),section));
    }
    void undo() override {
        // Member and schedule histories are separate. Restore removed references
        // only where the cell has not since been edited on the schedule stack.
        auto current=editorModel()->getSelectedIdsMap();
        for(auto it=beforeAssignments.cbegin();it!=beforeAssignments.cend();++it) {
            if(it.value()==afterAssignments.value(it.key()) || current.value(it.key())!=afterAssignments.value(it.key())) continue;
            const auto parts=it.key().split('_');
            const bool valid=parts.size()==3 && editorModel()->getTimesRow()->contains(parts[1])
                && (parts[0]=="0" ? editorModel()->getWeekColumns().contains(parts[2]) : parts[0]=="1" && editorModel()->getDateColumns().contains(parts[2]));
            if(valid)current[it.key()]=it.value();
        }
        ProjectManager::instance()->restoreMemberState(patchMembers(after,before),current);
    }
};
QJsonObject encode(const MemberInfoModel::MemberData& m) {
    QJsonObject map;
    for (auto day = m.scheduleMap.cbegin(); day != m.scheduleMap.cend(); ++day) {
        QJsonObject times; for(auto t=day.value().cbegin();t!=day.value().cend();++t) times[t.key()]=t.value();
        map[day.key()] = times;
    }
    return {{"id",m.id.toString()},{"properties",QJsonObject::fromVariantMap(m.properties)},{"note",m.note},
        {"scheduleDates",QJsonArray::fromStringList(m.scheduleDates)},{"scheduleTimes",QJsonArray::fromStringList(m.scheduleTimes)},
        {"scheduleMap",map},{"enableScheduling",m.enableScheduling},{"scheduleFileSource",m.scheduleFileSource}};
}
QString digest(const QString& path) {
    QFile f(path); if(!f.open(QIODevice::ReadOnly) || f.size()>256*1024) return {};
    return QString::fromLatin1(QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha256).toHex());
}
}
QJsonObject RabbitRollCommands::extended(const QString& op, const QJsonObject& args) {
    auto* project = ProjectManager::instance();
    if (op == "new_project" || op == "close_project") {
        if (!keys(args,Common+QStringList{"name","discard_unsaved"}) || (args.contains("discard_unsaved")&&!args["discard_unsaved"].isBool()))
            return failure("invalid_arguments","项目操作需要有效名称和布尔 discard_unsaved。");
        if (project->isDirty() && !args["discard_unsaved"].toBool()) return failure("unsaved_changes","当前项目有未保存修改；请先保存或明确 discard_unsaved=true。");
        if (op == "close_project") { project->closeProject(); return success({{"closed",true}}); }
        const auto name = args["name"].toString().trimmed();
        if (name.isEmpty() || name.size()>200) return failure("invalid_arguments","name 需要 1–200 字符。");
        project->newProject(name); return success();
    }
    if (op == "undo" || op == "redo") {
        const auto scope = args["scope"].toString();
        if (!keys(args,Common+QStringList{"scope"}) || (scope!="members" && scope!="schedule")) return failure("invalid_arguments","scope 为 members 或 schedule。");
        auto* stack = scope=="members" ? memberController()->getUndoStack() : editorController()->getUndoStack();
        if (op=="undo" ? !stack->canUndo() : !stack->canRedo()) return failure("nothing_to_undo","指定历史栈没有可执行操作。");
        const auto text = op=="undo" ? stack->undoText() : stack->redoText();
        if(op=="undo") stack->undo(); else stack->redo();
        return success({{"operation",text}});
    }
    if (op=="manage_members") {
        const auto action=args["action"].toString();
        if(!keys(args,Common+QStringList{"action","members","member_ids"})) return failure("invalid_arguments","未知成员管理字段。");
        auto section=project->memberExportSnapshot()["memberData"].toObject(); auto list=section["list"].toArray(); QJsonArray affected;
        if(action=="delete") {
            QStringList ids; if(!strings(args["member_ids"],ids,500)||ids.isEmpty()) return failure("invalid_arguments","member_ids 需要 1–500 个 UUID。");
            QSet<QUuid> selected;
            for(const auto& id:ids) { if(QUuid(id).isNull()||selected.contains(QUuid(id))||memberModel()->getRowById(QUuid(id))<0) return failure("member_not_found","成员不存在或重复。"); selected.insert(QUuid(id)); }
            for(int i=list.size()-1;i>=0;--i) if(selected.contains(QUuid(list[i].toObject()["id"].toString()))) { affected.append(list[i].toObject()["id"]); list.removeAt(i); }
        } else if(action=="add"||action=="update") {
            if(!args["members"].isArray()||args["members"].toArray().isEmpty()||args["members"].toArray().size()>500) return failure("invalid_arguments","members 需要 1–500 个对象。");
            QSet<QUuid> selected;
            for(const auto& v:args["members"].toArray()) {
                const auto item=v.toObject(); const auto props=item["properties"].toObject();
                if(!v.isObject()||!keys(item,{"member_id","properties","note","enabled"}) || (item.contains("properties")&&!item["properties"].isObject())
                    ||(item.contains("note")&&(!item["note"].isString()||item["note"].toString().size()>16384)) ||(item.contains("enabled")&&!item["enabled"].isBool()))
                    return failure("invalid_arguments","无效成员字段。");
                for(auto p=props.constBegin();p!=props.constEnd();++p) if(!memberController()->getAllPropertyNames().contains(p.key())||!p.value().isString()||p.value().toString().size()>4096||(p.key()=="姓名"&&p.value().toString().trimmed().isEmpty())) return failure("invalid_arguments","属性必须存在，值为字符串，姓名不能为空。");
                QJsonObject target; int row=-1;
                if(action=="add") {
                    if(item.contains("member_id")||props["姓名"].toString().trimmed().isEmpty()) return failure("invalid_arguments","新增成员需要 properties.姓名，UUID 自动生成。");
                    target=encode(memberModel()->createDefaultMemberData(props["姓名"].toString()));
                } else {
                    const QUuid id(item["member_id"].toString()); row=memberModel()->getRowById(id);
                    if(id.isNull()||row<0||selected.contains(id)) return failure("member_not_found","成员不存在或重复。"); selected.insert(id); target=list[row].toObject();
                }
                auto values=target["properties"].toObject(); for(auto p=props.constBegin();p!=props.constEnd();++p) values[p.key()]=p.value(); target["properties"]=values;
                if(item.contains("note")) target["note"]=item["note"];
                if(item.contains("enabled")) target["enableScheduling"]=item["enabled"];
                affected.append(target["id"]);
                if(row<0) list.append(target); else list[row]=target;
            }
        } else return failure("invalid_arguments","action 为 add、update 或 delete。");
        section["list"]=list;
        memberController()->getUndoStack()->push(new MemberStateCommand(section,"MCP 管理成员"));
        return success({{"member_ids",affected},{"member_count",list.size()}});
    }
    if(op=="edit_member_schedule") {
        QStringList dates,times; const QUuid id(args["member_id"].toString()); const int row=memberModel()->getRowById(id);
        if(!keys(args,Common+QStringList{"member_id","dates","time_ranges","cells"})||row<0||id.isNull()||!strings(args["dates"],dates,1000)||!strings(args["time_ranges"],times,100)||dates.size()*times.size()>10000||!args["cells"].isArray()||args["cells"].toArray().size()>10000)
            return failure("invalid_arguments","需要成员 UUID、完整 dates/time_ranges 和 cells 数组，最多10000格。");
        for(const auto& d:dates) if(!validDate(d)) return failure("invalid_arguments","日期无效。");
        for(const auto& t:times) if(!validTime(t)) return failure("invalid_arguments","时间段无效。");
        QJsonObject map; for(const auto& d:dates) { QJsonObject day; for(const auto& t:times) day[t]=false; map[d]=day; }
        QSet<QString> seen;
        for(const auto& v:args["cells"].toArray()) {
            const auto c=v.toObject(); const auto d=c["date"].toString(), t=c["time_range"].toString();
            if(!v.isObject()||!keys(c,{"date","time_range","busy"})||!dates.contains(d)||!times.contains(t)||!c["busy"].isBool()||seen.contains(d+'_'+t)) return failure("invalid_arguments","cells 位置无效或重复，busy 为布尔值，true 表示繁忙。");
            seen.insert(d+'_'+t); auto day=map[d].toObject(); day[t]=c["busy"]; map[d]=day;
        }
        auto section=project->memberExportSnapshot()["memberData"].toObject(); auto list=section["list"].toArray(); auto target=list[row].toObject();
        target["scheduleDates"]=QJsonArray::fromStringList(dates); target["scheduleTimes"]=QJsonArray::fromStringList(times); target["scheduleMap"]=map; list[row]=target;section["list"]=list;
        memberController()->getUndoStack()->push(new MemberStateCommand(section,"MCP 编辑个人时间表")); return success();
    }
    if(op=="apply_schedule_draft") {
        const auto id=args["draft_id"].toString();
        if(!keys(args,Common+QStringList{"draft_id","cell_keys"})||!drafts.contains(id)) return failure("draft_not_found","草案不存在或已过期。");
        const auto draft=drafts[id];
        if(draft.guard["project_id"]!=args["project_id"]||draft.guard["expected_revision"]!=args["expected_revision"]) return failure("revision_conflict","草案基于旧项目版本，请重新生成。");
        auto selected=draft.schedule;
        if(args.contains("cell_keys")) {
            QStringList cells; if(!strings(args["cell_keys"],cells,10000)||cells.isEmpty()) return failure("invalid_arguments","cell_keys 必须为非空数组。");
            selected.clear();for(const auto& key:cells){if(!draft.schedule.contains(key))return failure("invalid_arguments","草案中不存在该单元格。");selected[key]=draft.schedule[key];}
            if(!draft.properties.isEmpty()) return failure("invalid_arguments","包含成员属性修改的脚本草案只能整份应用。");
            auto all=editorModel()->getSelectedIdsMap(); const auto prefix=QString::number(editorModel()->getColumnMode())+'_';
            for(auto it=all.cbegin();it!=all.cend();++it)if(it.key().startsWith(prefix)&&!selected.contains(it.key()))selected.insert(it.key(),it.value());
        }
        const auto result=commitSchedule(selected,draft.properties); if(result["ok"].toBool())drafts.remove(id); return result;
    }
    if(op=="delete_script") {
        if(!keys(args,Common+QStringList{"name","expected_sha256"}))return failure("invalid_arguments","未知脚本字段。");
        const auto path=scriptPath(args["name"].toString());
        if(path.isEmpty())return failure("script_not_found","脚本不存在。");
        if(args["expected_sha256"].toString().isEmpty()||digest(path)!=args["expected_sha256"].toString())return failure("file_changed","脚本内容已变化，先 read_script 获取最新 sha256。");
        if(!QFile::remove(path))return failure("write_failed","无法删除脚本。");
        Q_EMIT ScriptEngine::instance()->scriptsChanged();return success();
    }
    return failure("unknown_operation","未实现的操作。");
}
QJsonObject RabbitRollCommands::startInputJob(const QString& op, const QJsonObject& args) {
    if(!keys(args,Common+QStringList{"path","mode","discard_unsaved"}) || !args["path"].isString())return failure("invalid_arguments","需要文件绝对路径。");
    const QFileInfo file(args["path"].toString());
    if(!file.isAbsolute()||!file.isFile()||file.size()>64*1024*1024||file.suffix().compare(op=="open_project"?"rroll":"rmem",Qt::CaseInsensitive)!=0)return failure("invalid_path","需要存在的 rroll/rmem 绝对路径，最多64MiB。");
    if(op=="open_project" && (args.contains("discard_unsaved")&&!args["discard_unsaved"].isBool()))return failure("invalid_arguments","discard_unsaved 为布尔值。");
    if(op=="open_project" && ProjectManager::instance()->isDirty()&&!args["discard_unsaved"].toBool())return failure("unsaved_changes","请先保存或明确丢弃当前未保存修改。");
    const auto mode=args["mode"].toString("replace");
    if(op=="import_rmem"&&mode!="replace"&&mode!="merge")return failure("invalid_arguments","mode 为 replace 或 merge。");
    while(jobs.size()>=64)jobs.erase(jobs.begin());
    auto job=std::make_shared<Job>();job->id=QUuid::createUuid().toString(QUuid::WithoutBraces);job->guard=args;job->operation=op;jobs[job->id]=job;runningJob=job->id;
    struct Input {QJsonObject root; QString error;};
    auto* watcher=new QFutureWatcher<Input>(this);
    connect(watcher,&QFutureWatcher<Input>::finished,this,[this,watcher,job,mode,file]{
        const auto input=watcher->result();watcher->deleteLater();if(runningJob==job->id)runningJob.clear();if(job->cancelled.load())return;
        auto fail=[&](const QJsonObject& e){job->state="failed";job->result=e;};
        if(!input.error.isEmpty()){fail(failure("invalid_file",input.error));return;}
        const auto check=precondition(job->guard,job->operation=="open_project");if(!check.isEmpty()){fail(check);return;}
        if(job->operation=="open_project") {
            if(!ProjectManager::instance()->replaceProject(input.root,file.absoluteFilePath())){fail(failure("invalid_file","项目结构无效。"));return;}
        } else {
            auto section=input.root["memberData"].toObject();
            if(mode=="merge") {
                auto previous=ProjectManager::instance()->memberExportSnapshot()["memberData"].toObject();auto list=previous["list"].toArray();auto names=previous["allProperties"].toArray();auto visibility=previous["visibility"].toObject();
                for(const auto& n:section["allProperties"].toArray())if(!names.contains(n)){names.append(n);visibility[n.toString()]=section["visibility"].toObject()[n.toString()];}
                for(const auto& v:section["list"].toArray()){int row=-1;for(int i=0;i<list.size();++i)if(QUuid(list[i].toObject()["id"].toString())==QUuid(v.toObject()["id"].toString())){row=i;break;}if(row<0)list.append(v);else list[row]=v;}
                previous["list"]=list;previous["allProperties"]=names;previous["visibility"]=visibility;section=previous;
            }
            memberController()->getUndoStack()->push(new MemberStateCommand(section,"MCP 导入成员文件"));
        }
        job->state="completed";job->result=success();
    });
    watcher->setFuture(QtConcurrent::run([file,op]{Input input;QFile f(file.absoluteFilePath());if(!f.open(QIODevice::ReadOnly)){input.error=f.errorString();return input;}const auto bytes=f.read(64*1024*1024+1);if(bytes.size()>64*1024*1024){input.error="文件过大。";return input;}const auto doc=QJsonDocument::fromJson(bytes);input.root=doc.object();if(!doc.isObject()||(op=="open_project"?!ProjectManager::validProject(input.root):!ProjectManager::validMembers(input.root)))input.error="文件结构无效。";return input;}));
    return success({{"job_id",job->id},{"state","running"}});
}
