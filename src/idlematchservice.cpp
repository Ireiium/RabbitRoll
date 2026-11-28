#include "idlematchservice.h"
#include <QtConcurrent>
#include <QDebug>
#include "componentmanager.h"
#include <atomic>
#include "projectsetting.h"
#include <QWriteLocker>

IdleMatchService* IdleMatchService::_instance = nullptr;
IdleMatchService* IdleMatchService::instance() {
    static IdleMatchService inst;
    return &inst;
}
IdleMatchService::IdleMatchService(QObject *parent)
	: QObject(parent), _isCalculating(false), _abortPending(0)
{
    _calcTimer = new QTimer(this);
    _calcTimer->setSingleShot(true);
    _calcTimer->setInterval(300);

    _searchStartDate = QDate::currentDate();
    _searchEndDate = _searchStartDate.addMonths(3);

    connect(_calcTimer, &QTimer::timeout, this, &IdleMatchService::realDoCalculation);

    connect(ProjectSetting::instance(), &ProjectSetting::toleranceChanged, this, &IdleMatchService::onDataChanged);
    connect(ProjectSetting::instance(), &ProjectSetting::searchRangeChanged, this, &IdleMatchService::onDataChanged);

    //后台运算完成时，写入缓存并通知UI
    connect(&_watcher, &QFutureWatcher<QHash<QString, QStringList>>::finished, this, [=]() {
        _futureActive = false;
        if (_abortPending.loadRelaxed() > 0 || _restartPending) {
            if (!_calcTimer->isActive()) _calcTimer->start(0);
            return;
        }

        _uiCache = _watcher.result();

        auto* eComp = ComponentManager::instance()->getComponent<EditorDutyComponent>();
        if (eComp) eComp->getController()->applyCalculationResults(_uiCache);

        _isCalculating.store(false);
        Q_EMIT precalculationFinished();
        /*qDebug() << "计算结果已更新至模型";*/
        });
}

IdleMatchService::~IdleMatchService()
{
    _calcTimer->stop();
    _abortPending.storeRelaxed(1);
    disconnect(&_watcher, nullptr, this, nullptr);
    _watcher.waitForFinished();
}

MemberBitSchedule::MemberBitSchedule() {
    for (int d = 0; d < 7; ++d) {
        for (int w = 0; w < 64; ++w) {
            weeklyMask[d][w].set();
        }
    }
}
TimeMask MemberBitSchedule::allIdleMask() {
    TimeMask mask;
    mask.set();
    return mask;
}

inline int IdleMatchService::popcount64(uint64_t x) {
#if defined(_MSC_VER)
    return (int)__popcnt64(x);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(x);
#else
    //备用算法
    int count = 0;
    while (x) { x &= (x - 1); count++; }
    return count;
#endif
}

//时间段转原子掩码
TimeMask IdleMatchService::timeStringToMask(const QString & timeRange) {
    TimeMask mask;
    QStringList parts = timeRange.split("-");
    if (parts.size() != 2) return mask;

    QTime startT = QTime::fromString(parts[0], "hh:mm");
    QTime endT = QTime::fromString(parts[1], "hh:mm");
    if (!startT.isValid() || !endT.isValid()) return mask;

    int startMin = startT.hour() * 60 + startT.minute();
    int endMin = endT.hour() * 60 + endT.minute();

    //把这段时间对应的分钟位全部置1
    for (int i = startMin; i < endMin; ++i) {
        if (i >= 0 && i < MINS_PER_DAY) {
            mask.set(i);
        }
    }
    return mask;
}

void IdleMatchService::onDataChanged() {
    _restartPending = true;
    _abortPending.storeRelaxed(1);
    _isCalculating.store(true);
    _calcTimer->start(300);
}

void IdleMatchService::realDoCalculation() {
    if (_futureActive) {
        _abortPending.storeRelaxed(1);//发送中止信号给当前正在跑的旧线程
        return;
    }
    auto* eComp = ComponentManager::instance()->getComponent<EditorDutyComponent>();
    auto* mComp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    if (!eComp || !mComp) {
        _restartPending = false;
        _isCalculating.store(false);
        return;
    }

    auto* eController = eComp->getController();
    auto* mController = mComp->getController();

    _isCalculating.store(true);
    _restartPending = false;
    _futureActive = true;
    _abortPending.storeRelaxed(0);

    ProjectSetting* settings = ProjectSetting::instance();
    QDate calcStart, calcEnd;
    if (settings->_enableSerchDateRange) {
        calcStart = settings->_searchStartDate;
        calcEnd = settings->_searchEndDate;
    }
    else {
        calcStart = QDate::currentDate();
        calcEnd = calcStart.addMonths(3);//默认从今天起到3个月后
    }
    _searchStartDate = calcStart;
    _searchEndDate = calcEnd;
	bool isEnableDateSearch = settings->_enableSerchDateRange;

    QList<MemberBitSchedule> snapshotDatas = mController->getMemberScheduleSnapshot();

    int currentMode = eController->getMode();
    QStringList editorTimes = eController->getTimesRow();
    QStringList editorCols = eController->getColumns();

    _tolerance = ProjectSetting::instance()->_toleranceValue;
    int tol = _tolerance;

    setBaseDate(mController->calculateEarliestMonday());
    QDate baseSnapshot = getBaseDate();

    /*qDebug() << "开始计算";*/
    QFuture<QHash<QString, QStringList>> future = QtConcurrent::run(
        &IdleMatchService::runPrecalculation,
        this,
        snapshotDatas,
        baseSnapshot,
        calcStart,
        calcEnd,
        isEnableDateSearch,
        tol,
        editorTimes,
        editorCols,
        currentMode
    );
    _watcher.setFuture(future);
}

QHash<QString, QStringList> IdleMatchService::runPrecalculation(
    IdleMatchService* service,
    QList<MemberBitSchedule> members, 
    QDate baseDate, 
    QDate startDate, 
    QDate endDate, 
    bool isEnableDateSearch,
    int tolerance,
    QStringList editorTimes, 
    QStringList editorCols, 
    int editorMode)
{
    QHash<QString, QStringList> resultCache;
    if (!startDate.isValid() || !endDate.isValid() || startDate > endDate) return resultCache;
    static QMap<QString, int> weekTextToNum = {
        {"周一", 1}, {"周二", 2}, {"周三", 3}, {"周四", 4}, {"周五", 5}, {"周六", 6}, {"周日", 7}
    };


    //时间行 -> 天 -> 成员筛选
    for (const QString& colName : editorCols) {
        if (service->_abortPending.loadRelaxed() > 0) return QHash<QString, QStringList>();

        //如果是日期模式，先检查日期并检查范围
        QDate targetDate;
        bool isDateOutOfRange = false;
        if (editorMode == 1) {
            targetDate = QDate::fromString(colName, "yyyy-MM-dd");
            if (!targetDate.isValid() || (isEnableDateSearch && (targetDate < startDate || targetDate > endDate))) {
                isDateOutOfRange = true;
            }
        }

        int targetDayOfWeek = weekTextToNum.value(colName, -1);
        bool hasRequiredDay = editorMode == 1 || (targetDayOfWeek >= 1 &&
            startDate.addDays((targetDayOfWeek - startDate.dayOfWeek() + 7) % 7) <= endDate);

        for (const QString& timeStr : editorTimes) {
            QString cacheKey = QString("%1_%2_%3").arg(editorMode).arg(colName).arg(timeStr);
            QStringList availableNames;
            //如果日期超限，直接跳过成员循环，该时段availableNames保持为空
            if (isDateOutOfRange) {
                resultCache[cacheKey] = availableNames;
                continue;
            }
            TimeMask queryMask = timeStringToMask(timeStr);
            if (queryMask.none() || !hasRequiredDay) {
                resultCache[cacheKey] = availableNames;
                continue;
            }

            for (const auto& member : members) {
                if (service->_abortPending.loadRelaxed() > 0) return {};
                if (matchesAvailability(member, editorMode, colName, queryMask,
                        startDate, endDate, isEnableDateSearch, tolerance)) {
                    availableNames << member.id.toString();
                }
            }
            resultCache[cacheKey] = availableNames;
        }
    }
    return resultCache;
}

QStringList IdleMatchService::getAvailableMembers(int mode, const QString& target, const QString& timeRange) {
    QString key = QString("%1_%2_%3").arg(mode).arg(target).arg(timeRange);
    return _uiCache.value(key);
}

bool IdleMatchService::isCalculating() const {
    return _isCalculating.load();
}

void IdleMatchService::setBaseDate(const QDate& date) {
    _baseDateLock.lockForWrite();

    if (_baseDate != date) {
        _baseDate = date;
    }
    _baseDateLock.unlock();
}

QDate IdleMatchService::getBaseDate() const {
    _baseDateLock.lockForRead();

    QDate tempDate = _baseDate;

    _baseDateLock.unlock();

    return tempDate;
}

// No cached UI state is read here. Both callers use the same minute masks and
// the same project date range/tolerance semantics.
bool IdleMatchService::matchesAvailability(const MemberBitSchedule& member, int mode,
    const QString& target, const TimeMask& mask, const QDate& start, const QDate& end,
    bool restrictDates, int tolerance) {
    if (mask.none() || !start.isValid() || !end.isValid() || start > end) return false;
    if (mode == 1) {
        const auto date = QDate::fromString(target, "yyyy-MM-dd");
        if (!date.isValid() || (restrictDates && (date < start || date > end))) return false;
        const auto day = member.dailyMask.value(date, MemberBitSchedule::allIdleMask());
        return (day & mask) == mask;
    }
    const QStringList days{"周一", "周二", "周三", "周四", "周五", "周六", "周日"};
    const int weekday = days.indexOf(target) + 1;
    if (weekday < 1 || start.addDays((weekday - start.dayOfWeek() + 7) % 7) > end) return false;
    int conflicts = 0;
    for (auto it = member.dailyMask.cbegin(); it != member.dailyMask.cend(); ++it) {
        if (it.key() >= start && it.key() <= end && it.key().dayOfWeek() == weekday
            && (it.value() & mask) != mask && ++conflicts > tolerance) return false;
    }
    return true;
}
