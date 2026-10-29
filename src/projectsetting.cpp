#include "projectsetting.h"
#include "projectmanager.h"

ProjectSetting* ProjectSetting::_instance = nullptr;

ProjectSetting* ProjectSetting::instance() {
    if (!_instance) {
        _instance = new ProjectSetting();
    }
    return _instance;
}

ProjectSetting::ProjectSetting(QObject* parent)
	: QObject(parent), _toleranceValue(0)
{
}

ProjectSetting::~ProjectSetting()
{
}

//数据重置为默认值（新建项目时调用）
void ProjectSetting::reset() {
    _projectName = "未命名项目";
    _toleranceValue = 0;
    _searchStartDate = QDate::currentDate();
    _searchEndDate = QDate::currentDate().addMonths(3);
    _enableSerchDateRange = false;
}

void ProjectSetting::setToleranceValue(int val) {
    if (val >= 0 && val <= 5) {
        _toleranceValue = val;
        ProjectManager::instance()->setDirty(true);
        Q_EMIT toleranceChanged();
    }
}

void ProjectSetting::setSerachDateRange(const QDate& start, const QDate& end) {
    if (_searchStartDate != start || _searchEndDate != end) {
        if(start.isValid() && end.isValid() && start <= end) {
            _searchStartDate = start;
            _searchEndDate = end;
            ProjectManager::instance()->setDirty(true);
            Q_EMIT searchRangeChanged();
		}
    }
}

void ProjectSetting::enableSerachDateRange(bool enable) {
    if (_enableSerchDateRange != enable) {
        _enableSerchDateRange = enable;
        ProjectManager::instance()->setDirty(true);
        Q_EMIT searchRangeChanged();
    }
}