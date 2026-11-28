#include "backendworker.h"
#include "AIServiceInterface.h"
#include <QJsonArray>
#include <QJsonDocument>

void BackendWorker::runAISchedule(const QString& prompt, const QHash<QString,QStringList>& candidates,
    const QList<MemberInfoModel::MemberData>& members, bool thinking) {
    QString error;
    const auto result = calculateAISchedule(prompt,candidates,members,2,thinking,&_abort,error);
    Q_EMIT aiScheduleFinished(error.isEmpty(),result,error);
}

QHash<QString,QList<QUuid>> BackendWorker::calculateAISchedule(const QString& prompt,
    const QHash<QString,QStringList>& candidates,const QList<MemberInfoModel::MemberData>& members,
    int targetPerCell,bool thinking,const std::atomic<bool>* cancelled,QString& error) {
    error.clear();QMap<QUuid,QString> aliases;QMap<QString,QUuid> ids;QJsonObject people,slots;QMap<QString,QString> slotKeys;
    int n=0;for(const auto& member:members){if(!member.enableScheduling)continue;const auto alias="M"+QString::number(++n);aliases[member.id]=alias;ids[alias]=member.id;people[alias]=QJsonObject{{"properties",QJsonObject::fromVariantMap(member.properties)},{"duty_count",member.dutyCount}};}
    QStringList keys=candidates.keys();keys.sort();n=0;
    for(const auto& key:keys){QJsonArray allowed;for(const auto& id:candidates[key])if(aliases.contains(QUuid(id)))allowed.append(aliases[QUuid(id)]);if(allowed.isEmpty())continue;const auto slot="T"+QString::number(++n);slotKeys[slot]=key;slots[slot]=QJsonObject{{"position",key},{"candidates",allowed}};}
    if(slots.isEmpty()){error="没有可排班候选人。";return {};}
    const QString message="按候选人排班，每格最多"+QString::number(targetPerCell)+"人。只返回JSON对象，键为T编号，值为M编号数组，不得使用候选名单以外的人。\n数据："+
        QString::fromUtf8(QJsonDocument(QJsonObject{{"members",people},{"slots",slots}}).toJson(QJsonDocument::Compact))+"\n用户要求："+prompt;
    if(message.toUtf8().size()>100000){error="提示词超过100000字节，请缩小项目范围。";return {};}
    if(cancelled&&cancelled->load()){error="任务已取消。";return {};}
    auto response=AIServiceInterface::instance()->generateResponseStreaming(message,thinking,cancelled).trimmed();
    if(cancelled&&cancelled->load()){error="任务已取消。";return {};}
    if(response.startsWith("API Error:")||response.startsWith("HTTP Error")){error=response;return {};}
    if(response.startsWith("```")){const auto first=response.indexOf('\n');response=response.mid(first+1);if(response.endsWith("```"))response.chop(3);}
    const auto doc=QJsonDocument::fromJson(response.toUtf8());if(!doc.isObject()||doc.object().isEmpty()){error="AI未返回有效的排班JSON对象。";return {};}
    QHash<QString,QList<QUuid>> result;
    const auto object=doc.object();for(auto it=object.constBegin();it!=object.constEnd();++it){if(!slotKeys.contains(it.key())||!it.value().isArray()||it.value().toArray().size()>targetPerCell){error="AI返回了未知位置或人数超限。";return {};}
        QList<QUuid> selected;for(const auto& alias:it.value().toArray()){const auto id=ids.value(alias.toString());if(!alias.isString()||id.isNull()||selected.contains(id)||!candidates[slotKeys[it.key()]].contains(id.toString())){error="AI返回了未知、重复或不空闲的成员。";return {};}selected.append(id);}result[slotKeys[it.key()]]=selected;}
    return result;
}
