#pragma once
#include "componentmanager.h"
#include "projectmanager.h"
#include "projectsetting.h"
#include "ipcprotocol.h"
namespace RabbitRollBusiness {
extern const QStringList Days;
MemberInfoController* memberController();
MemberInfoModel* memberModel();
EditorDutyController* editorController();
EditorDutyModel* editorModel();
QJsonObject success(const QJsonObject& data = {});
bool keys(const QJsonObject& obj, const QStringList& allowed);
bool validDate(const QString& text);
bool validTime(const QString& text);
bool strings(const QJsonValue& value, QStringList& out, int max);
QString scriptPath(const QString& name);
QJsonObject precondition(const QJsonObject& args, bool allowEmpty = false);
}
