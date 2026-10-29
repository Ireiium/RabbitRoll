#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDate>

class ProjectSetting : public QObject
{
    Q_OBJECT

Q_SIGNALS:
    void projectNameChanged();//通知名字改了
    void toleranceChanged();//通知容忍度改了
    void searchRangeChanged();//通知时间范围改了

public:
    static ProjectSetting* instance();
    ~ProjectSetting();

    QString _projectName;
    QString _windowGeometry;
    QString _windowState;
    QString _themeMode;
    QStringList _recentFiles;
    int _toleranceValue{ 0 };
    QDate _searchStartDate;//搜索范围起点
    QDate _searchEndDate;//搜索范围终点
	bool _enableSerchDateRange{ false };

    void reset();
    void setToleranceValue(int val);
	void setSerachDateRange(const QDate& start, const QDate& end);
    void enableSerachDateRange(bool enable);

private:
    explicit ProjectSetting(QObject* parent = nullptr);
    static ProjectSetting* _instance;
};