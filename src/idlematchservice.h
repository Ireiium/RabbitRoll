#pragma once

#include <QObject>
#include <QStringList>
#include <bitset>
#include <QHash>
#include <QMap>
#include <QDate>
#include <QTime>
#include <QFutureWatcher>
#include <QUuid>
#include <QReadWriteLock>
#include <QTimer>
#include <atomic>
#include <array>

const int MINS_PER_DAY = 1440;//一天1440分钟
using TimeMask = std::bitset<MINS_PER_DAY>;
struct MemberBitSchedule {
    QString name;    
    QUuid id;
    
    QHash<QDate, TimeMask> dailyMask;//日期模式：Key为具体日期，value为这天的1440分钟空闲位图
    std::array<std::array<TimeMask, 64>, 7> weeklyMask;//周模式：[周几(1-7)][第几周(0-63)] = 这天的1440分钟空闲位图

    MemberBitSchedule();

    static TimeMask allIdleMask();//为一个新日期提供全1的掩码

};

class IdleMatchService  : public QObject
{
	Q_OBJECT

public Q_SLOTS:
    void onDataChanged();

Q_SIGNALS:
    void precalculationFinished();

public:
	IdleMatchService(QObject *parent=nullptr);
	~IdleMatchService();
    static IdleMatchService* instance();

    /*void setDateRange(const QDate& start, const QDate& end);
    void setTolerance(int tolerance);*/

    QStringList getAvailableMembers(int mode, const QString& target, const QString& timeRange);

    bool isCalculating() const;

    
    static TimeMask timeStringToMask(const QString& timeRange);//将时间转为timeMask

    void setBaseDate(const QDate& date);
    QDate getBaseDate() const;

private:
    static IdleMatchService* _instance;

    QTimer* _calcTimer;
    void realDoCalculation();

    QDate _baseDate;//基准日期（用于计算周索引，必须为周一）
    mutable QReadWriteLock _baseDateLock;//保护baseDate并发读写
    QDate _searchStartDate;//搜索范围起点
    QDate _searchEndDate;//搜索范围终点
    int _tolerance{ 0 };//宽容度

    QList<MemberBitSchedule> _memberBitDatas;//数据快照

    QHash<QString, QStringList> _uiCache;
    QFutureWatcher<QHash<QString, QStringList>> _watcher;

    static QHash<QString, QStringList>runPrecalculation(
        IdleMatchService* service,//检查状态
        QList<MemberBitSchedule> members,
        QDate baseDate,
        QDate startDate, 
        QDate endDate, 
        bool isEnableDateSearch,
        int tolerance,
        QStringList editorTimes, 
        QStringList editorCols, 
        int editorMode);


    static inline int popcount64(uint64_t x);//获取1的个数

    std::atomic<bool> _isCalculating{ false };
    QAtomicInt _abortPending;
    bool _futureActive{ false };
    bool _restartPending{ false };


};

//DeepSeek MoE模型 2026.4.13
//AI教的
/*时间轴（分钟级原子化）：
一天有 24 小时 = 1440 分钟。
不用管用户分了多个时间段。直接用一个拥有1440个坑位的“位集合”std::bitset<1440>
如08:00是第480分钟，10:00 是第600分钟。
当成员在08:00到10:00有空时，把他的第480到599位全部置为1。
如果需要查09:00-10:00，只需检查540到599位是否为 1。
周期轴（周级宽容度）：
利用uint64_t的64个位，代表64个星期。一行位运算就能算出跨越几个月的出勤率。*/

