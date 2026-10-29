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
        if (_abortPending.loadRelaxed() > 0) {
            _isCalculating.store(false);
            return;
        }

        _uiCache = _watcher.result();

        auto* eComp = ComponentManager::instance()->getComponent<EditorDutyComponent>();
        eComp->getController()->applyCalculationResults(_uiCache);

        _isCalculating.store(false);
        Q_EMIT precalculationFinished();
        /*qDebug() << "计算结果已更新至模型";*/
        });
}

IdleMatchService::~IdleMatchService()
{}

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
    _calcTimer->start();
}

void IdleMatchService::realDoCalculation() {
    if (_watcher.isRunning()) {
        _abortPending.storeRelaxed(1);//发送中止信号给当前正在跑的旧线程
        _calcTimer->start(50);
        return;
    }
    auto* eComp = ComponentManager::instance()->getComponent<EditorDutyComponent>();
    auto* mComp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    if (!eComp || !mComp) return;//疑似组件还没初始化完成的时候就会跑一次，所以防御返回

    auto* eController = eComp->getController();
    auto* mController = mComp->getController();

    _isCalculating.store(true);
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
            if (isEnableDateSearch && (targetDate < startDate || targetDate > endDate)) {
                isDateOutOfRange = true;
            }
        }

        //预算周掩码
        uint64_t requiredWeekMask = 0;
        int totalRequiredWeeks = 0;

        if (editorMode != 1) { //周模式才需要算掩码
            int targetDayOfWeek = weekTextToNum.value(colName, -1);
            if (targetDayOfWeek != -1) {
                QDate tempDate = startDate;
                while (tempDate <= endDate) {
                    if (tempDate.dayOfWeek() == targetDayOfWeek) {
                        int weekIdx = baseDate.daysTo(tempDate) / 7;
                        if (weekIdx >= 0 && weekIdx < 64) {
                            requiredWeekMask |= (1ULL << weekIdx);
                            totalRequiredWeeks++;
                        }
                    }
                    tempDate = tempDate.addDays(1);
                }
            }
        }

        for (const QString& timeStr : editorTimes) {
            QString cacheKey = QString("%1_%2_%3").arg(editorMode).arg(colName).arg(timeStr);
            QStringList availableNames;
            //如果日期超限，直接跳过成员循环，该时段availableNames保持为空
            if (isDateOutOfRange) {
                resultCache[cacheKey] = availableNames;
                continue;
            }
            TimeMask queryMask = timeStringToMask(timeStr);

            for (const auto& member : members) {
                if (editorMode == 1) {
                    TimeMask dayMask = member.dailyMask.contains(targetDate) ?
                        member.dailyMask.value(targetDate) :
                        MemberBitSchedule::allIdleMask();

                    if ((dayMask & queryMask) == queryMask) {
                        availableNames << member.id.toString();
                    }
                }
                else {//周期模式
                    int dayOfWeek = weekTextToNum.value(colName, -1);
                    if (dayOfWeek >= 1 && dayOfWeek <= 7) {
                        int conflictWeeks = 0;
                        const auto& weekData = member.weeklyMask[dayOfWeek - 1];

                        for (int w = 0; w < 64; ++w) {
                            if (requiredWeekMask & (1ULL << w)) {
                                if ((weekData[w] & queryMask) != queryMask) {
                                    conflictWeeks++;
                                }
                            }
                        }
                        if (conflictWeeks <= tolerance) {
                            availableNames << member.id.toString();
                        }
                    }
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
