#include "editordutycontroller.h"
#include "componentmanager.h"
#include "editordutyview.h"
#include "editordutymodel.h"
#include <QLayout>
#include <QDate>
#include "memberinfocomponent.h"
#include "backendworker.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include "undocommands.h"
#include <QFile>
#include <QFileInfo>
#include <random>
#include <algorithm>   

EditorDutyController::EditorDutyController(EditorDutyModel* model, EditorDutyView* view, QObject* parent)
    : QObject(parent), _model(model), _view(view)
{
    _undoStack = new QUndoStack(this);

    connect(_view, &EditorDutyView::doubleClicked, this, [=](const QModelIndex& index) {
        if (index.isValid()) {
            Q_EMIT cellDoubleClicked(index.row(), index.column());
        }
        });

    connect(_model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& topLeft, const QModelIndex& bottomRight) {
            for (int r = topLeft.row(); r <= bottomRight.row(); ++r) {
                for (int c = topLeft.column(); c <= bottomRight.column(); ++c) {
                    Q_EMIT cellDataChanged(r, c);
                }
            }
        });

    connect(BackendWorker::instance(), &BackendWorker::autoScheduleFinished, this, &EditorDutyController::applyAutoScheduleResults);
    connect(BackendWorker::instance(), &BackendWorker::scriptScheduleFinished, this, &EditorDutyController::applyPythonScheduleResults);
#ifdef ENABLE_AI_SUPPORT
    connect(BackendWorker::instance(), &BackendWorker::aiScheduleFinished, this, &EditorDutyController::applyAIResult);
#endif

}

void EditorDutyController::setupView(QLayout* layout) {
    if (layout) {
        layout->addWidget(_view);
    }
}

void EditorDutyController::addTimeRow(const int hour1, const int minute1, const int hour2, const int minute2) {
    QString time = QString::asprintf("%02d:%02d-%02d:%02d", hour1, minute1, hour2, minute2);
    if (hour1 > hour2 || (hour1 == hour2 && minute1 > minute2)) {
        qDebug() << "时间不合法";
        Q_EMIT addTimeError(RTypies::Message::TimeIsNotValid);
    }
    else if (_model->getTimesRow()->contains(time)) {
        qDebug() << "时间重复";
        Q_EMIT addTimeError(RTypies::Message::DuplicateAdd);
    }
    else {
        // _model->addTimeRow(time, QModelIndex(), row, row);
        _undoStack->push(new EditorDutyAddTimeCommand(this, time));
    }
}

void EditorDutyController::addColumn() {
    QString title;
    if (_model->getColumnMode() == EditorDutyModel::WeekMode) {
        QStringList weekDays = { "周一", "周二", "周三", "周四", "周五", "周六", "周日" };
        QStringList currentColumns = _model->getWeekColumns();

        for (const QString& day : weekDays) {
            if (!currentColumns.contains(day)) {
                title = day;
                break;
            }
        }
        if (title.isEmpty()) {
            qDebug() << "一周七天已全部添加";
            return;
        }
    }
    else {
        QStringList currentCols = _model->getDateColumns();
        if (currentCols.isEmpty()) {
            title = QDate::currentDate().toString("yyyy-MM-dd");
        }
        else {
            QString lastDateStr = currentCols.last();
            QDate lastDate = QDate::fromString(lastDateStr, "yyyy-MM-dd");

            if (lastDate.isValid()) {
                QDate nextDate = lastDate.addDays(1);

                while (currentCols.contains(nextDate.toString("yyyy-MM-dd"))) {
                    nextDate = nextDate.addDays(1);
                }
                title = nextDate.toString("yyyy-MM-dd");
            }
            else {
                title = QDate::currentDate().toString("yyyy-MM-dd");
            }
        }
    }

    if (!title.isEmpty()) {
        // _model->addColumn(title);
        EditorDutyModel::ColumnMode currentMode = _model->getColumnMode();
        _undoStack->push(new EditorDutyAddColumnCommand(this, currentMode, title));
    }
}

//void EditorDutyController::switchMode(EditorDutyModel::ColumnMode mode) {
//    _model->setColumnMode(mode);
//}

int EditorDutyController::getMode() const{
    if (_model->getColumnMode() == EditorDutyModel::WeekMode) {
        return 0;
    }
    else {
        return 1;
    }
}
QStringList EditorDutyController::getTimesRow() const {
    QStringList* listPtr = _model->getTimesRow();
    return listPtr ? *listPtr : QStringList();
}

QStringList EditorDutyController::getColumns() const {
    if (_model->getColumnMode() == EditorDutyModel::WeekMode) {
        return _model->getWeekColumns();
    }
    else {
        return _model->getDateColumns();
    }
}

void EditorDutyController::switchMode() {
    EditorDutyModel::ColumnMode current = _model->getColumnMode();
    EditorDutyModel::ColumnMode target = (current == EditorDutyModel::WeekMode) ? EditorDutyModel::DateMode : EditorDutyModel::WeekMode;

    _model->setColumnMode(target);

    // qDebug() << "模式已切换为:" << (target == EditorDutyModel::WeekMode ? "周模式" : "日期模式");
}

QModelIndex EditorDutyController::getIndex(int row, int col) {
    return _model->index(row, col);
}

void EditorDutyController::updateViewHeight() {
    _view->updateHeightToContents();
}

void EditorDutyController::applyCalculationResults(const QHash<QString, QStringList>& schedule) {
    auto* mComp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    auto* mController = mComp->getController();

    int mode = (int)_model->getColumnMode();
    QStringList rows = *(_model->getTimesRow());
    QStringList cols = (mode == 0) ? _model->getWeekColumns() : _model->getDateColumns();

    _model->blockSignals(true);

    QList<QUuid> allValidIds = mController->getAllValidIds();

    for (int r = 0; r < rows.count(); ++r) {
        for (int c = 0; c < cols.count(); ++c) {
            QString key = QString("%1_%2_%3").arg(mode).arg(cols[c]).arg(rows[r]);

            QStringList availableIdsStr = schedule.value(key, QStringList());
            _model->setCellMembers(r, c, availableIdsStr);

            QSet<QUuid> availableIdSet;
            for (const QString& idStr : availableIdsStr) availableIdSet.insert(QUuid(idStr));

			//处理被删除的
            QList<QUuid> currentSelectedIds = _model->getSelectedIds(r, c);
            bool changed = false;
            for (int i = currentSelectedIds.size() - 1; i >= 0; --i) {
                QUuid selectedId = currentSelectedIds[i];

                if (!allValidIds.contains(selectedId) || !availableIdSet.contains(selectedId)) {
                    currentSelectedIds.removeAt(i);
                    changed = true;
                }
            }

            if (changed) {
                _model->setSelectedIds(r, c, currentSelectedIds);
            }
        }
    }

    _model->blockSignals(false);
    _model->refreshView();
}

QList<QUuid> EditorDutyController::getSelectedIds(const QModelIndex& index) const {
    if (!index.isValid()) return {};
    return _model->getSelectedIds(index.row(), index.column());
}

void EditorDutyController::setSelectedIds(const QModelIndex& index, const QList<QUuid>& ids) {
    if (!index.isValid()) return;
    QList<QUuid> oldIds = _model->getSelectedIds(index.row(), index.column());
    if (oldIds == ids) return;
    _undoStack->push(new EditorDutySetCellCommand(this, index.row(), index.column(), oldIds, ids));
}

void EditorDutyController::editTimeRow(int index, const int hour1, const int minute1, const int hour2, const int minute2) {
    if (index < 0 || index >= _model->rowCount()) return;
    QString newTime = QString::asprintf("%02d:%02d-%02d:%02d", hour1, minute1, hour2, minute2);
    if (hour1 > hour2 || (hour1 == hour2 && minute1 > minute2)) {
        Q_EMIT addTimeError(RTypies::Message::TimeIsNotValid);
        return;
    }
    if (_model->getTimesRow()->contains(newTime) && _model->getTimesRow()->at(index) != newTime) {
        Q_EMIT addTimeError(RTypies::Message::DuplicateAdd);
        return;
    }
    QString oldTime = _model->getTimesRow()->at(index);
    if (oldTime == newTime) return;
    _undoStack->push(new EditorDutyEditTimeCommand(this, oldTime, newTime));
}

void EditorDutyController::handleEditRow(int index) {
    if (index < 0 || index >= _model->rowCount()) return;

    QString timeStr = _model->getTimesRow()->at(index);
    int h1, m1, h2, m2;
    if (sscanf(timeStr.toStdString().c_str(), "%d:%d-%d:%d", &h1, &m1, &h2, &m2) == 4) {
        QVariantList editData;
        editData << index << h1 << m1 << h2 << m2;
        Q_EMIT popWinDialogRequest(RTypies::WinDialog::EditEditorTimeRow, editData);
    }
}

void EditorDutyController::autoSchedule(int targetPerCell, bool enablePrioritize, bool isMultipate) {

    QHash<QString, QStringList> availableMap = _model->getCellDataMap();

    QHash<QString, QStringList> currentModeMap;
    QString modePrefix = QString::number((int)_model->getColumnMode()) + "_";
    for (auto it = availableMap.begin(); it != availableMap.end(); ++it) {
        if (it.key().startsWith(modePrefix)) {
            currentModeMap.insert(it.key(), it.value());
        }
    }

    using AvailableMapType = QHash<QString, QStringList>;
    QMetaObject::invokeMethod(BackendWorker::instance(), [=]() {
        BackendWorker::instance()->autoSchedule(
            currentModeMap,
            targetPerCell,
            enablePrioritize,
            isMultipate,
            static_cast<int>(_model->getColumnMode())
            );
    }, Qt::QueuedConnection);
}

void EditorDutyController::applyAutoScheduleResults(const QHash<QString, QList<QUuid>>& schedule) {
    _model->setSelectedIdsMap(schedule);
    _model->refreshView();
    Q_EMIT scheduleFinish(RTypies::Message::Success, "");

}

void EditorDutyController::PythonSchedule(const QString& scriptPath, int targetPerCell) {
    if (!QFile::exists(scriptPath)) {
        QString errorMsg = QString("脚本文件丢失或无法访问: %1\n请前往设置页面重新选择有效的脚本！").arg(QFileInfo(scriptPath).fileName());
        Q_EMIT scheduleFinish(RTypies::Message::Failure, errorMsg);
        return;
    }

    auto* mComp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    auto* mModel = mComp->getModel();
    QList<MemberInfoModel::MemberData> allMembersData;
    for (int i = 0; i < mModel->rowCount(); ++i) {
        MemberInfoModel::MemberData* ptr = mModel->getMemberDataPtr(i);
        if (ptr) {
            allMembersData.append(*ptr);
        }
    }
    QHash<QString, QStringList> availableMap = _model->getCellDataMap();
    QHash<QString, QStringList> currentModeMap;
    QString modePrefix = QString::number((int)_model->getColumnMode()) + "_";

    for (auto it = availableMap.begin(); it != availableMap.end(); ++it) {
        if (it.key().startsWith(modePrefix)) {
            currentModeMap.insert(it.key(), it.value());
        }
    }

    using AvailableMapType = QHash<QString, QStringList>;
    using MemberDataListType = QList<MemberInfoModel::MemberData>;
    QMetaObject::invokeMethod(BackendWorker::instance(), [=]() {
        BackendWorker::instance()->scriptSchedule(
            scriptPath,
            targetPerCell,
            currentModeMap,
            allMembersData
            );
    }, Qt::QueuedConnection);
}

void EditorDutyController::applyPythonScheduleResults(const QHash<QString, QList<QUuid>>& schedule, const QString& debugMsg) {
    if (!debugMsg.isEmpty()) {
        qDebug() << "Python脚本错误:" << debugMsg;
        Q_EMIT scheduleFinish(RTypies::Message::Failure, debugMsg);
        return;
    }

    _model->setSelectedIdsMap(schedule);
    _model->refreshView();

    Q_EMIT scheduleFinish(RTypies::Message::Success, "");
}

void EditorDutyController::applyAIResult(bool success, const QHash<QString, QList<QUuid>>& schedule,
    const QString& debugMsg) {
    if (success) {
        _model->setSelectedIdsMap(schedule);
        _model->refreshView();
        Q_EMIT scheduleFinish(RTypies::Message::Success, "");
    }
    else {
        Q_EMIT scheduleFinish(RTypies::Message::Failure, debugMsg);
    }
}

int EditorDutyController::getMemberDutyCount(const QUuid& id) const {
    return _model->getMemberDutyCount(id);
}

void EditorDutyController::handleClearCellsRequest(const QModelIndexList& indices) {
    if (indices.isEmpty()) return;

    _undoStack->beginMacro("清除单元格内容");
    for (const QModelIndex& index : indices) {
        if (index.isValid()) {
            QList<QUuid> oldIds = _model->getSelectedIds(index.row(), index.column());
            if (!oldIds.isEmpty()) {
                _undoStack->push(new EditorDutySetCellCommand(this, index.row(), index.column(), oldIds, QList<QUuid>()));
            }
        }
    }
    _undoStack->endMacro();
}

static QVariantMap uuidMapToVariant(const QMap<QString, QUuid>& map) {
    QVariantMap vMap;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        vMap.insert(it.key(), it.value().toString());
    }
    return vMap;
}

static QVariantMap stringMapToVariant(const QMap<QString, QString>& map) {
    QVariantMap vMap;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        vMap.insert(it.key(), it.value());
    }
    return vMap;
}

void EditorDutyController::abortSchedule() {
    BackendWorker::instance()->abortCurrentTask();
}

QUndoStack* EditorDutyController::getUndoStack() const { 
    return _undoStack; 
}

void EditorDutyController::deleteTimeRow(int index) {
    if (index < 0 || index >= _model->rowCount()) return;
    QString timeStr = _model->getTimesRow()->at(index);
    auto selMap = _model->getSelectedIdsMap();
    auto cellMap = _model->getCellDataMap();
    QHash<QString, QList<QUuid>> savedSelIds;
    QHash<QString, QStringList> savedCellData;
    for (auto it = selMap.begin(); it != selMap.end(); ++it) {
        if (it.key().split('_').value(1) == timeStr) savedSelIds.insert(it.key(), it.value());
    }
    for (auto it = cellMap.begin(); it != cellMap.end(); ++it) {
        if (it.key().split('_').value(1) == timeStr) savedCellData.insert(it.key(), it.value());
    }

    _undoStack->push(new EditorDutyRemoveRowCommand(this, timeStr, savedSelIds, savedCellData));
}

void EditorDutyController::deleteTimeRows(const QList<int>& rows) {
    if (rows.isEmpty()) return;
    _undoStack->beginMacro("批量删除排班时间段");
    QList<int> sortedRows = rows;
    std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());
    for (int row : sortedRows) {
        deleteTimeRow(row);
    }
    _undoStack->endMacro();
}

void EditorDutyController::deleteColumn(int index) {
    if (index < 0 || index >= _model->columnCount()) return;
    int mode = (int)_model->getColumnMode();
    QString colStr = (mode == 0) ? _model->getWeekColumns().at(index) : _model->getDateColumns().at(index);
    auto selMap = _model->getSelectedIdsMap();
    auto cellMap = _model->getCellDataMap();
    QHash<QString, QList<QUuid>> savedSelIds;
    QHash<QString, QStringList> savedCellData;
    for (auto it = selMap.begin(); it != selMap.end(); ++it) {
        if (it.key().split('_').value(2) == colStr && it.key().split('_').value(0).toInt() == mode) {
            savedSelIds.insert(it.key(), it.value());
        }
    }
    for (auto it = cellMap.begin(); it != cellMap.end(); ++it) {
        if (it.key().split('_').value(2) == colStr && it.key().split('_').value(0).toInt() == mode) {
            savedCellData.insert(it.key(), it.value());
        }
    }
    _undoStack->push(new EditorDutyRemoveColumnCommand(this, mode, colStr, savedSelIds, savedCellData));
}

void EditorDutyController::deleteColumns(const QList<int>& columns) {
    if (columns.isEmpty()) return;
    _undoStack->beginMacro("批量删除排班列");
    QList<int> sortedCols = columns;
    std::sort(sortedCols.begin(), sortedCols.end(), std::greater<int>());
    for (int col : sortedCols) {
        deleteColumn(col);
    }
    _undoStack->endMacro();
}

void EditorDutyController::updateColumnDate(int index, const QDate& newDate) {
    if (!_model || index < 0 || index >= _model->columnCount()) return;
    QString oldDateStr = _model->getFullDateString(index);
    QDate oldDate = QDate::fromString(oldDateStr, "yyyy-MM-dd");
    _undoStack->push(new EditorDutyEditDateCommand(this, oldDate, newDate));
}

void EditorDutyController::setSelectedIdsDirectly(int row, int col, const QList<QUuid>& ids) {
    _model->setSelectedIds(row, col, ids); 
}
void EditorDutyController::addTimeRowDirectly(const QString& time) { 
    _model->addTimeRow(time, QModelIndex(), 0, 0); 
}
void EditorDutyController::removeTimeRowDirectly(const QString& time) { 
    _model->removeTimeRowByString(time); 
}
void EditorDutyController::restoreTimeRowDirectly(const QString& time, const QHash<QString, QStringList>& cellData, const QHash<QString, QList<QUuid>>& selIds) {
    _model->addTimeRow(time, QModelIndex(), 0, 0);
    _model->restoreCellData(cellData, selIds);
}
void EditorDutyController::addColumnDirectly(int mode, const QString& colTitle) {
    if ((int)_model->getColumnMode() != mode) _model->setColumnMode(static_cast<EditorDutyModel::ColumnMode>(mode));
    _model->addColumn(colTitle);
}
void EditorDutyController::removeColumnDirectly(int mode, const QString& colTitle) {
    if ((int)_model->getColumnMode() != mode) _model->setColumnMode(static_cast<EditorDutyModel::ColumnMode>(mode));
    _model->removeColumnByString(colTitle);
}
void EditorDutyController::restoreColumnDirectly(int mode, const QString& colTitle, const QHash<QString, QStringList>& cellData, const QHash<QString, QList<QUuid>>& selIds) {
    if ((int)_model->getColumnMode() != mode) _model->setColumnMode(static_cast<EditorDutyModel::ColumnMode>(mode));
    _model->addColumn(colTitle);
    _model->restoreCellData(cellData, selIds);
}
void EditorDutyController::updateTimeRowDirectly(const QString& oldTime, const QString& newTime) { 
    _model->updateTimeRowByString(oldTime, newTime);
}
void EditorDutyController::updateDateSlotDirectly(const QDate& oldDate, const QDate& newDate) {
    _model->updateDateSlotByString(oldDate.toString("yyyy-MM-dd"), newDate);
}

#ifdef ENABLE_AI_SUPPORT
void EditorDutyController::requestAISchedule(const QString& userPrompt, QDate startDate, QDate endDate,
    bool enableThinking, const QSet<QString>& properties) {
    qDebug() << "AI排班启动";
    BackendWorker::instance()->resetAbort();
    auto* memberModel = ComponentManager::instance()->getComponent<MemberInfoComponent>()->getModel();

    QMap<QString, QUuid> nameToUuidMap;
    QString memberListStr = "可用成员：\n";
    for (int i = 0; i < memberModel->rowCount(); ++i) {
        auto* data = memberModel->getMemberDataPtr(i);
        if (!data) continue;
        QString name = data->properties.value("姓名").toString();
        nameToUuidMap[name] = data->id;

        QString extra;
        if (!properties.isEmpty()) {
            QStringList propValues;
            for (const QString& prop : properties) {
                if (!data->properties.contains(prop) || data->properties.value(prop).toString().isEmpty())
                    continue;
                propValues << QString("%1: %2").arg(prop).arg(data->properties.value(prop).toString());
            }
            if (!propValues.isEmpty()) {
                extra = "，" + propValues.join("，");
            }
        }
        memberListStr += QString("%1（已排班 %2 次%3）\n").arg(name).arg(data->dutyCount).arg(extra);
    }

    QMap<QString, QString> taskSlotToKeyMap;
    QString taskSlotStr = "\n任务槽：\n";
    int taskIndex = 0;
    auto cellMap = _model->getCellDataMap();
    int currentModeInt = static_cast<int>(_model->getColumnMode());
    int colCount = _model->columnCount();
    int rowCount = _model->rowCount();

    for (int col = 0; col < colCount; ++col) {
        QString colId = _model->headerData(col, Qt::Horizontal).toString();
        if (currentModeInt == 1) {
            QDate colDate = QDate::fromString(colId, "yyyy-MM-dd");
            if (colDate.isValid() && (colDate < startDate || colDate > endDate))
                continue;
        }

        for (int row = 0; row < rowCount; ++row) {
            QString timeStr = _model->getTimesRow()->at(row);
            QString key = QString("%1_%2_%3").arg(currentModeInt).arg(timeStr).arg(colId);
            if (!cellMap.contains(key) || cellMap[key].isEmpty())
                continue;

            QString slotDesc = QString("%1_%2").arg(colId).arg(timeStr);
            taskSlotToKeyMap[slotDesc] = key;

            //可选成员
            QStringList candNames;
            for (const QString& uuidStr : cellMap[key]) {
                QUuid uid(uuidStr);
                QString mName = memberModel->getNameById(uid);
                if (!mName.isEmpty()) candNames << mName;
            }
            //随机打乱顺序
            static std::random_device rd;
            static std::mt19937 g(rd());
            std::shuffle(candNames.begin(), candNames.end(), g);

			bool limitCandidates = false;//true-限制提取人数，false-不限制
            if (memberModel->rowCount() <= 5) 
                limitCandidates = false;

            if (limitCandidates && candNames.size() > 5) {
                candNames = candNames.first(5);
            }

            taskSlotStr += QString("[%1] 可选: %2\n").arg(slotDesc).arg(candNames.join(", "));
        }
    }

    if (taskSlotToKeyMap.isEmpty()) {
        Q_EMIT scheduleFinish(RTypies::Message::Failure, "没有可排班的成员");
        return;
    }

    QString compressedData = memberListStr + taskSlotStr;

    QMetaObject::invokeMethod(BackendWorker::instance(), [=]() {
        BackendWorker::instance()->AISchedule(
            userPrompt,
            compressedData,
            nameToUuidMap,
            taskSlotToKeyMap,
            enableThinking
        );
        }, Qt::QueuedConnection);
}
#endif