#include "commandcontext.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
namespace RabbitRollBusiness {
using namespace RabbitRollMcp;
const QStringList Days{"周一","周二","周三","周四","周五","周六","周日"};
MemberInfoController* memberController() { return ComponentManager::instance()->getComponent<MemberInfoComponent>()->getController(); }
MemberInfoModel* memberModel() { return ComponentManager::instance()->getComponent<MemberInfoComponent>()->getModel(); }
EditorDutyController* editorController() { return ComponentManager::instance()->getComponent<EditorDutyComponent>()->getController(); }
EditorDutyModel* editorModel() { return ComponentManager::instance()->getComponent<EditorDutyComponent>()->getModel(); }
QJsonObject success(const QJsonObject& data) {
    auto* p = ProjectManager::instance();
    return {{"ok",true},{"project_id",p->projectSessionId()},
        {"revision",QString::number(p->changeRevision())},{"data",data}};
}
bool keys(const QJsonObject& obj, const QStringList& allowed) {
    for (const auto& key : obj.keys()) if (!allowed.contains(key)) return false;
    return true;
}
bool validDate(const QString& text) {
    const auto date = QDate::fromString(text, "yyyy-MM-dd");
    return date.isValid() && date.toString("yyyy-MM-dd") == text;
}
bool validTime(const QString& text) {
    static const QRegularExpression re("^([01][0-9]|2[0-3]):[0-5][0-9]-([01][0-9]|2[0-3]):[0-5][0-9]$");
    return re.match(text).hasMatch() && text.left(5) < text.right(5);
}
bool strings(const QJsonValue& value, QStringList& out, int max) {
    if (!value.isArray() || value.toArray().size() > max) return false;
    for (const auto& item : value.toArray()) {
        if (!item.isString() || out.contains(item.toString())) return false;
        out.append(item.toString());
    }
    return true;
}
QString scriptPath(const QString& name) {
    if (name.isEmpty() || QFileInfo(name).fileName() != name || name.contains('/') || name.contains('\\')
        || !name.endsWith(".py",Qt::CaseInsensitive)) return {};
    const QDir scripts(QCoreApplication::applicationDirPath() + "/scripts");
    const QFileInfo file(scripts.filePath(name));
    if (!file.isFile() || file.isSymLink() || !file.canonicalFilePath().startsWith(
        scripts.canonicalPath() + '/',Qt::CaseInsensitive)) return {};
    return file.canonicalFilePath();
}
QJsonObject precondition(const QJsonObject& args, bool allowEmpty) {
    auto* p = ProjectManager::instance();
    if (p->isLoading()) return failure("project_loading","正在切换项目，请稍后重试。");
    if (p->isSaving()) return failure("busy","界面正在保存项目，请完成后重试。");
    if (!p->hasProject() && !allowEmpty) return failure("no_project","请先打开或新建项目。");
    if (!args["project_id"].isString() || (!allowEmpty && args["project_id"].toString().isEmpty())
        || !args["expected_revision"].isString())
        return failure("invalid_arguments","写操作需要 project_id 和 expected_revision 字符串，先读取 get_project_context。");
    if (args["project_id"].toString() != p->projectSessionId()) return failure("project_changed","项目已切换，未应用修改。");
    if (!(allowEmpty && !p->hasProject() && args["expected_revision"] == "") && args["expected_revision"].toString() != QString::number(p->changeRevision()))
        return failure("revision_conflict","项目已被修改，请重新读取数据后重试。");
    if ((!allowEmpty || p->hasProject()) && IdleMatchService::instance()->isCalculating()) return failure("calculating","空闲数据正在更新，请稍后重新读取项目版本。");
    if (editorController()->isScheduling()) return failure("busy","界面正在排班，请完成后重试。");
    return {};
}
}
