#pragma once

#include <QObject>
#include "ElaScrollPage.h"

class ElaToggleSwitch;
class SettingPage  : public ElaScrollPage
{
	Q_OBJECT

Q_SIGNALS:
	void enableScriptChanged(bool enabled, QString script);
	void scriptChanged(QString script);
	void changeWinMode(int mode);
	void clearRecentFiles();
	void resetSettings();

#ifdef ENABLE_AI_SUPPORT
	void enableAIChanged(bool enabled);
#endif

public:
	SettingPage(bool hasAI, QWidget* parent);
	~SettingPage();

	bool getIsScriptMode() const;
	QString getScript() const;

private:
	bool _isEnabledScript{ false };
	QString _script{ "" };

    ElaToggleSwitch* _enableScriptSwitch{ nullptr};
#ifdef ENABLE_AI_SUPPORT
	bool _isEnabledAI;
	ElaToggleSwitch* _enableAISwitch{ nullptr };
#endif
};

