#include "ipcclient.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QtMcpServer/QMcpServer>
#include <QtMcpCommon/QMcpTextContent>
#include <cstdio>
#include <QPromise>
#include <QProcess>
#include <QFileInfo>

class RabbitRollTools : public QMcpServer {
    Q_OBJECT
public:
    RabbitRollTools() : QMcpServer("stdio"), client(this) {
        setInstructions("RabbitRoll 正在编辑的内存项目工具。先读取 get_project_context、list_members、get_schedule；无项目用new_project/open_project。公共版本字段指project_id、expected_revision、request_id。"
            "写工具的 request_json 是 JSON 对象字符串，必须带 project_id、expected_revision（来自 context.revision）和唯一 request_id。"
            "编辑修改可在界面撤销，不自动保存；save_project 保存项目，导出工具写出文件。重复提交完全相同的 request_id 不会重复执行。"
            "calculating/busy 时等待后重新读取版本，revision_conflict/project_changed 时重新读取数据。"
            "导入、排班、导出和保存返回 job_id，用 get_job_status 轮询 state=completed/failed/cancelled；检查其 result。"
            "运行脚本使用 list_scripts 返回的名称；write_script 可管理 scripts 目录的脚本，不接受任意脚本路径。");
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> get_project_context() {
        return call("get_project_context", {});
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> query_availability(
        const QString& mode, const QString& target, const QString& time_range,
        const QString& member_id = {}, const QString& project_id = {}) {
        return call("query_availability", {{"mode", mode}, {"target", target},
            {"time_range", time_range}, {"member_id", member_id}, {"project_id", project_id}});
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> validate_schedule(const QString& project_id = {}) {
        return call("validate_schedule", {{"project_id", project_id}});
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> list_members(const QString& project_id = {}) {
        return call("list_members", {{"project_id",project_id}});
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> get_schedule(const QString& project_id = {}) {
        return call("get_schedule", {{"project_id",project_id}});
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> list_scripts() { return call("list_scripts", {}); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> get_job_status(const QString& job_id) { return call("get_job_status", {{"job_id",job_id}}); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> update_member_properties(const QString& request_json) {
        return callJson("update_member_properties",request_json);
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> manage_properties(const QString& request_json) {
        return callJson("manage_properties",request_json);
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> configure_schedule(const QString& request_json) {
        return callJson("configure_schedule",request_json);
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> assign_members(const QString& request_json) {
        return callJson("assign_members",request_json);
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> import_rsch(const QString& request_json) {
        return callJson("import_rsch",request_json);
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> auto_schedule(const QString& request_json) {
        return callJson("auto_schedule",request_json);
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> run_schedule_script(const QString& request_json) {
        return callJson("run_schedule_script",request_json);
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> export_excel(const QString& request_json) {
        return callJson("export_excel",request_json);
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> export_rmem(const QString& request_json) {
        return callJson("export_rmem",request_json);
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> save_project(const QString& request_json) {
        return callJson("save_project",request_json);
    }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> new_project(const QString& request_json) { return callJson("new_project",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> open_project(const QString& request_json) { return callJson("open_project",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> close_project(const QString& request_json) { return callJson("close_project",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> manage_members(const QString& request_json) { return callJson("manage_members",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> edit_member_schedule(const QString& request_json) { return callJson("edit_member_schedule",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> import_rmem(const QString& request_json) { return callJson("import_rmem",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> export_rsch(const QString& request_json) { return callJson("export_rsch",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> undo(const QString& request_json) { return callJson("undo",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> redo(const QString& request_json) { return callJson("redo",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> preview_schedule(const QString& request_json) { return callJson("preview_schedule",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> apply_schedule_draft(const QString& request_json) { return callJson("apply_schedule_draft",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> write_script(const QString& request_json) { return callJson("write_script",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> delete_script(const QString& request_json) { return callJson("delete_script",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> ai_schedule(const QString& request_json) { return callJson("ai_schedule",request_json); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> get_member_schedule(const QString& member_id) { return call("get_member_schedule", {{"member_id",member_id}}); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> cancel_job(const QString& job_id) { return call("cancel_job", {{"job_id",job_id}}); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> get_schedule_draft(const QString& draft_id) { return call("get_schedule_draft", {{"draft_id",draft_id}}); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> read_script(const QString& name) { return call("read_script", {{"name",name}}); }
    Q_INVOKABLE QFuture<QList<QMcpCallToolResultContent>> start_rabbitroll() {
        return client.request("get_project_context",{}).then([](const QJsonObject& result) {
            const auto code=result["error"].toObject()["code"].toString();QJsonObject answer;
            if(result["ok"].toBool()||code=="no_project"||code=="project_loading")answer={{"ok",true},{"data",QJsonObject{{"state","already_running"}}}};
            else if(code!="unavailable"&&code!="disconnected")answer=result;
            else {
                const auto executable=QCoreApplication::applicationDirPath()+"/RabbitRoll.exe";
                QProcess process;process.setProgram(executable);process.setWorkingDirectory(QCoreApplication::applicationDirPath());
                process.setStandardOutputFile(QProcess::nullDevice());process.setStandardErrorFile(QProcess::nullDevice());qint64 pid=0;
                if(!QFileInfo::exists(executable)||!process.startDetached(&pid))answer=RabbitRollMcp::failure("launch_failed","无法启动同目录的RabbitRoll.exe。");
                else answer={{"ok",true},{"data",QJsonObject{{"state","launch_requested"},{"message","已请求启动GUI；MCP随程序默认开启，请稍后重新读取项目概况。"}}}};
            }
            return QList<QMcpCallToolResultContent>{QMcpTextContent(QString::fromUtf8(QJsonDocument(answer).toJson(QJsonDocument::Compact)))};
        });
    }
    QHash<QString, QString> toolDescriptions() const override {
        return {{"new_project","新建空项目。request_json 需 project_id、expected_revision、request_id 和 name；有未保存修改默认拒绝，明确丢弃可传 discard_unsaved:true。无项目时 project_id/expected_revision 可用空字符串。"},
            {"open_project","打开或切换到指定项目，返回job_id。request_json：公共版本字段和path（rroll绝对路径），可选discard_unsaved:true；默认拒绝丢弃修改。无项目可用空版本字段。"},
            {"close_project","关闭当前项目而不退出RabbitRoll。request_json：公共版本字段，可选discard_unsaved:true；默认拒绝丢弃修改。"},
            {"manage_members","完整成员管理，一次可撤销。request_json：公共版本字段，action:add/update/delete。add/update 用 members:[{member_id?:UUID,properties?:{已有属性名:字符串},note?:字符串,enabled?:布尔值}]；add必须有properties.姓名且UUID自动生成，update必须有member_id。delete用member_ids:[UUID]，删除会清除引用，撤销可恢复。"},
            {"get_member_schedule","读取成员完整个人时间表、备注和参与排班状态；member_id为UUID，scheduleMap中true为繁忙。"},
            {"edit_member_schedule","整次替换成员个人时间表，可撤销；读原表后修改。request_json：公共版本字段、member_id、dates:[yyyy-MM-dd]、time_ranges:[HH:mm-HH:mm]、cells:[{date,time_range,busy:布尔值}]。未列入cells的格子为空闲；移除行列即可删除，空数组可清空。"},
            {"import_rmem","导入完整成员文件，返回job_id，支持一次撤销。request_json：公共版本字段、path绝对rmem路径、mode?:replace默认值或merge。replace替换成员并清除失效引用；merge按UUID更新/追加，不按姓名合并。"},
            {"export_rsch","导出指定成员个人时间表为现有加密RSCH格式，可重新导入。request_json：公共版本字段、member_id、path绝对rsch路径、overwrite?:false。RSCH表示日期时段忙闲，不能存储多人值班分配；多人分配用Excel/rroll。返回job_id。"},
            {"undo","执行撤销。request_json：公共版本字段及scope:members或schedule。文件写入不属于撤销历史。"},
            {"redo","执行重做。request_json：公共版本字段及scope:members或schedule。"},
            {"cancel_job","按job_id取消单个任务；已完成任务保持完成状态。取消后不应用结果/不提交文件。"},
            {"preview_schedule","生成排班草案，不修改项目，返回job_id，完成结果含draft_id。request_json：公共版本字段、method:auto/script/ai、target_per_cell及对应自动/脚本/AI工具的选项。"},
            {"get_schedule_draft","按draft_id读取草案cells和属性修改、基准版本。单元格key格式为模式_时间段_日期或星期。"},
            {"apply_schedule_draft","应用草案，一次可撤销。request_json：公共版本字段、draft_id、可选cell_keys:[草案key]选择部分应用；含属性修改的草案只支持整份应用。项目变化后拒绝旧草案。"},
            {"read_script","读取scripts目录现有脚本，参数name为文件名；返回code和sha256，不接受目录路径。"},
            {"write_script","创建/修改scripts目录中的py脚本，语法校验后写入，不执行代码；返回job_id。request_json：公共版本字段、name文件名、code（最多128KiB）、overwrite?:false、expected_sha256?:原脚本哈希。覆盖需overwrite:true及read_script最新哈希。脚本文件写入不可由项目撤销。"},
            {"delete_script","删除scripts目录脚本，不可由项目撤销。request_json：公共版本字段、name文件名、expected_sha256（read_script最新哈希）。"},
            {"ai_schedule","使用RabbitRoll已配置的内置AI排班，返回job_id，结果一次可撤销。request_json：公共版本字段、prompt字符串、target_per_cell整数、thinking?:false、preview?:false。需ENABLE_AI构建、界面已启用AI且已配置key；MCP不读取或返回密钥。"},
            {"start_rabbitroll","启动同目录RabbitRoll.exe；stdout不继承MCP管道。已连接时不重复启动。MCP随GUI默认开启；只返回启动请求状态，就绪后重新读取项目概况。"},
            {"export_excel","导出当前内存中的排班表和/或界面可见的成员属性列为 Excel，不改变项目状态；返回 job_id。request_json：{project_id:string,expected_revision:string,request_id:string,path:输出xlsx绝对路径,scope?:字符串schedule默认值或members或both,overwrite?:bool默认false}。默认不覆盖已有文件；任务完成后 data.result.data.path 为实际路径。"},
            {"export_rmem","导出完整成员数据（含UUID、隐藏属性、个人时间表、备注、参与排班设置）为 .rmem，不改变项目状态；返回 job_id。request_json：{project_id:string,expected_revision:string,request_id:string,path:输出rmem绝对路径,overwrite?:bool默认false}。"},
            {"save_project","将当前内存项目保存为 .rroll，成功后更新当前路径和已保存状态，保留撤销历史；返回 job_id。request_json：{project_id:string,expected_revision:string,request_id:string,path?:输出rroll绝对路径,overwrite?:bool默认false}。省略path为保存到当前文件（允许覆盖）；未保存项目需要path。提供path为另存为，覆盖已有文件须overwrite=true。任务生成期间项目发生变化或MCP停用则不提交文件。"},
            {"list_members","读取成员稳定 UUID、全部属性和项目属性名称，供编辑前定位成员。"},
            {"list_members/project_id","可选：当前项目会话 ID。"},
            {"get_schedule","读取当前排班的全部单元格、日期/星期、时间段及成员 UUID。"},
            {"get_schedule/project_id","可选：当前项目会话 ID。"},
            {"list_scripts","列出 RabbitRoll 安装目录 scripts 中可运行的现有 .py 脚本名称。"},
            {"get_job_status","读取后台导入/排班任务的状态及结果；state 为 running/completed/failed/cancelled。completed 表示已完成内存修改或输出文件提交。"},
            {"get_job_status/job_id","导入/排班工具返回的 job_id。"},
            {"update_member_properties","批量修改一个或多个成员的属性，整批校验、一次撤销。request_json 结构：{project_id:string,expected_revision:string,request_id:string,changes:[{member_id:UUID,properties:{属性名:string}}]}。属性必须已经存在，姓名不能为空。"},
            {"manage_properties","添加或删除全局成员属性，可撤销。request_json：{project_id:string,expected_revision:string,request_id:string,action:字符串add或remove,names:[属性名]}。名称不重复、最多8字，保留姓名/序号。"},
            {"configure_schedule","设置完整的排班时间行及当前模式日期/星期列，移除的行列中的分配也会移除，可整次撤销。request_json：{project_id:string,expected_revision:string,request_id:string,mode:字符串date或weekday,columns:[yyyy-MM-dd或周一至周日],time_ranges:[HH:mm-HH:mm],search_range?:{enabled:bool,start:yyyy-MM-dd,end:yyyy-MM-dd,tolerance_weeks:0到5整数}}。未提供 search_range 则保留原设置。"},
            {"assign_members","批量设置具体单元格的完整人员列表，空列表清空；复用界面空闲候选规则，整次可撤销。request_json：{project_id:string,expected_revision:string,request_id:string,cells:[{target:yyyy-MM-dd或周一至周日,time_range:HH:mm-HH:mm,member_ids:[UUID]}]}。位置必须已存在。"},
            {"import_rsch","一键批量导入 RSCH：后台使用界面同一校验及成员匹配规则；任一文件有问题则整批不导入，错误结果包含各文件原因；返回 job_id。request_json：{project_id:string,expected_revision:string,request_id:string,paths:[rsch绝对路径],format?:字符串默认姓名,semester_start?:第一周周一yyyy-MM-dd,existing_only?:bool默认false,replace_schedule?:bool默认false,overwrite_properties?:bool默认false,targets?:{绝对路径:已有成员UUID或字符串new}}。仅访问指定 RSCH 文件，不接受目录或其他格式。"},
            {"auto_schedule","复用界面一键排班算法，完成后替换当前模式分配并形成一次撤销；返回 job_id。request_json：{project_id:string,expected_revision:string,request_id:string,target_per_cell:1到500整数,prioritize_fewer?:bool默认false,allow_multiple?:bool默认false}。参数对应界面的每格人数、次数少优先及周模式允许多次。"},
            {"run_schedule_script","运行 list_scripts 中的现有脚本，完成后将分配及已存在的成员属性更新作为一次可撤销操作应用；返回 job_id。request_json：{project_id:string,expected_revision:string,request_id:string,script_name:现有脚本文件名,target_per_cell:1到500整数}。普通 Python 循环限时10秒，失败不应用结果；不接受任意路径或脚本代码。"},
            {"get_project_context", "读取正在编辑的内存项目概况（包括未保存修改）、排班模式、时间范围与成员数；无项目返回 no_project。"},
            {"query_availability", "复用 RabbitRoll 空闲规则查询成员，返回稳定 UUID 与姓名。个人表未标忙的时间视为空闲；星期查询使用项目时间范围和容忍周数，不排除已分配的班次。"},
            {"query_availability/mode", "date 或 weekday；应与项目排班模式一致。"},
            {"query_availability/target", "date 模式为 yyyy-MM-dd；weekday 模式为周一、周二、周三、周四、周五、周六或周日。"},
            {"query_availability/time_range", "HH:mm-HH:mm，同日且起点早于终点，例如 08:00-10:00。"},
            {"query_availability/member_id", "可选：仅返回此 UUID 成员；空字符串表示所有参与排班的成员。"},
            {"query_availability/project_id", "可选：get_project_context 返回的项目会话 ID，切换后返回 project_changed。"},
            {"validate_schedule", "检查当前模式的排班：失效成员引用、重复分配、未参与排班、与个人繁忙时间冲突、同一成员的重叠班次和无效位置。未配置人数、公平性等规则不作假设。"},
            {"validate_schedule/project_id", "可选：项目会话 ID。"}};
    }
private:
    QFuture<QList<QMcpCallToolResultContent>> callJson(const QString& op, const QString& text) {
        const auto document = QJsonDocument::fromJson(text.toUtf8());
        if (document.isObject()) return call(op,document.object());
        QPromise<QList<QMcpCallToolResultContent>> promise;
        promise.start();
        promise.addResult({QMcpTextContent(QString::fromUtf8(QJsonDocument(RabbitRollMcp::failure(
            "invalid_arguments", "request_json 必须为 JSON 对象字符串。")).toJson(QJsonDocument::Compact)))});
        promise.finish();
        return promise.future();
    }
    QFuture<QList<QMcpCallToolResultContent>> call(const QString& op, const QJsonObject& args) {
        return client.request(op, args).then([](const QJsonObject& obj) {
            return QList<QMcpCallToolResultContent>{QMcpTextContent(
                QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)))};
        });
    }
    RabbitRollIpcClient client;
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("RabbitRollMcp");
    // Qt diagnostics always go to stderr; stdout belongs exclusively to QtMcp.
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& msg) {
        fprintf(stderr, "%s\n", msg.toUtf8().constData());
        fflush(stderr);
    });
    if (!QMcpServer::backends().contains("stdio")) {
        fprintf(stderr, "Missing mcpserverbackend/qmcpserverstdio.dll\n");
        return 2;
    }
    RabbitRollTools server;
    QObject::connect(&server, &QMcpServer::finished, &app, &QCoreApplication::quit);
    QObject::connect(&server, &QMcpServer::errorOccurred, &app, [&app](const QString& error) {
        fprintf(stderr, "%s\n", error.toUtf8().constData());
        app.exit(2);
    });
    server.start();
    return app.exec();
}
#include "helpermain.moc"
