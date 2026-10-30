#include "schedulecontroller.h"
#include "schedulemodel.h"
#include "scheduleview.h"
#include "memberinfomodel.h"
#include <QDate>
#include <QMessageBox>
#include "undocommands.h"
#include "memberinfocontroller.h"
#include <QTime>
#include "componentmanager.h"

ScheduleController::ScheduleController(ScheduleModel* model, ScheduleView* view, QObject* parent)
    : QObject(parent), _model(model), _view(view)
{
    connect(_model,&ScheduleModel::editTimeError,this,[=](){
        Q_EMIT addNewTimeRowFail(RTypies::Message::DuplicateAdd);
		});
}

void ScheduleController::setupView(QLayout* layout) {
    if (layout && _view) {
        layout->addWidget(_view);
    }
}

void ScheduleController::requestToggleState(const QModelIndex& index) {
    if (!index.isValid() || !_model || !_mController || _currentMemberId.isNull()
        || index.model() != _model || index.row() >= _model->rowCount()
        || index.column() >= _model->columnCount()) return;

    QString date = _model->getFullDateString(index.column());
    QString time = _model->getTimesRow()->at(index.row());

    _mController->getUndoStack()->push(new ScheduleToggleCommand(_currentMemberId, date, time, _mController, this));
}

void ScheduleController::addNextDay() {
    if (!_model || !_mController || _currentMemberId.isNull()) return;

    QString nextDateStr;
    int col = _model->columnCount();

    if (col == 0) {
        nextDateStr = QDate::currentDate().toString("yyyy-MM-dd");
    }
    else {
        QString lastDateStr = _model->getLastDate();
        QDate lastDate = QDate::fromString(lastDateStr, "yyyy-MM-dd");
		/*qDebug() << "Last date string: " << lastDateStr << ", Parsed date: " << lastDate.toString("yyyy-MM-dd");*/

        if (lastDate.isValid()) {
            nextDateStr = lastDate.addDays(1).toString("yyyy-MM-dd");
        }
        else {
            nextDateStr = QDate::currentDate().toString("yyyy-MM-dd");
        }
    }

    if (!nextDateStr.isEmpty()) {
        _mController->getUndoStack()->push(new ScheduleAddDateCommand(_currentMemberId, nextDateStr, _mController, this));
    }
}

void ScheduleController::addTimeSlot(const int hour1, const int minute1, const int hour2, const int minute2) {
    if (!_model || !_mController || _currentMemberId.isNull() || !_model->getTimesRow()) return;
    QString time = QString::asprintf("%02d:%02d-%02d:%02d", hour1, minute1, hour2, minute2);
    if (!QTime(hour1, minute1).isValid() || !QTime(hour2, minute2).isValid()
        || QTime(hour1, minute1) >= QTime(hour2, minute2)) {
        qDebug() << "时间不合法";
        addNewTimeRowFail(RTypies::Message::TimeIsNotValid);
    }
    else if (_model->getTimesRow()->contains(time)) {
        qDebug() << "时间重复";
        addNewTimeRowFail(RTypies::Message::DuplicateAdd);
    }
    else {
        _mController->getUndoStack()->push(new ScheduleAddTimeCommand(_currentMemberId, time, _mController, this));
    }
}
int ScheduleController::columnCount() const {
    return _model ? _model->columnCount() : 0;
}

int ScheduleController::rowCount() const {
    return _model ? _model->rowCount() : 0;
}

void ScheduleController::onMemberDataSelected(void* data) {
    if (!_model) return;
    if (!data) {
        _currentMemberId = QUuid();
        _model->setScheduleData(nullptr, nullptr, nullptr);
        return;
    }
    auto* memberData = static_cast<MemberInfoModel::MemberData*>(data);
    _currentMemberId = memberData->id;
    auto* members = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    _model->bindMember(members ? members->getModel() : nullptr, _currentMemberId);
}

void ScheduleController::updateViewHeight() {
    _view->updateViewHeight();
}

void ScheduleController::editTimeSlot(int index, const int hour1, const int minute1, const int hour2, const int minute2) {
    if (!_model || !_mController || _currentMemberId.isNull() || index < 0 || index >= _model->rowCount()) return;

    QString newTime = QString::asprintf("%02d:%02d-%02d:%02d", hour1, minute1, hour2, minute2);

    if (!QTime(hour1, minute1).isValid() || !QTime(hour2, minute2).isValid()
        || QTime(hour1, minute1) >= QTime(hour2, minute2)) {
        Q_EMIT addNewTimeRowFail(RTypies::Message::TimeIsNotValid);
        return;
    }

    QStringList* times = _model->getTimesRow();
    for (int i = 0; i < times->size(); ++i) {
        if (i != index && times->at(i) == newTime) {
            Q_EMIT addNewTimeRowFail(RTypies::Message::DuplicateAdd);
            return;
        }
    }
    QString oldTime = times->at(index);
    if (oldTime == newTime) return;
    _mController->getUndoStack()->push(new ScheduleEditTimeCommand(_currentMemberId, index, oldTime, newTime, _mController, this));
}

void ScheduleController::handleEditRowRequest(int logicalIndex) {
    if (!_model || logicalIndex < 0 || logicalIndex >= _model->rowCount()) return;

    QString timeStr = _model->getTimesRow()->at(logicalIndex);
    int h1, m1, h2, m2;
    if (sscanf(timeStr.toStdString().c_str(), "%d:%d-%d:%d", &h1, &m1, &h2, &m2) == 4) {
        QList<int> editData;
        editData << logicalIndex << h1 << m1 << h2 << m2;
        Q_EMIT popWinDialogRequest(RTypies::WinDialog::EditMemberTimeRow, QVariant::fromValue(editData));
    }
}

bool ScheduleController::handleImportedSchedule(const QJsonObject& importedData,
    QWidget* parentWidget, int overwritePolicy) {
    if (!_model || importedData.isEmpty()) return false;
    bool overwrite = overwritePolicy == 1;
    if (overwritePolicy < 0 && _model->checkConflict(importedData)) {
        int ret = QMessageBox::question(parentWidget, "检测到冲突",
            "导入的课表与当前时间表存在冲突。\n选择“Yes”将覆盖冲突的时间段为忙碌，选择“No”将跳过冲突部分。", 
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (ret == QMessageBox::Cancel) return false;
        overwrite = (ret == QMessageBox::Yes);
    }
    _model->mergeSchedule(importedData, overwrite);
    updateViewHeight();
    return true;
}

void ScheduleController::handleClearCellsRequest(const QModelIndexList& indices) {
    if (indices.isEmpty()) return;

    QList<QPair<QString, QString>> targetCells;
    for (const QModelIndex& index : indices) {
        if (!index.isValid()) continue;
        if (index.data(Qt::UserRole).toBool()) {
            QString date = _model->getFullDateString(index.column());
            QString time = _model->getTimesRow()->at(index.row());
            targetCells.append({ date, time });
        }
    }

    if (!targetCells.isEmpty()) {
        _mController->getUndoStack()->push(new ScheduleClearCellsCommand(_currentMemberId, targetCells, _mController, this));
    }
}

void ScheduleController::setMemberInfoController(MemberInfoController* mController) {
    _mController = mController;
}

void ScheduleController::toggleStateDirectly(const QString& date, const QString& time) {
    _model->toggleStateByString(date, time);
}

void ScheduleController::addTimeSlotDirectly(const QString& time) {
    int row = _model->rowCount();
    _model->addTimeSlot(time, QModelIndex(), row, row);
}

void ScheduleController::removeTimeSlotDirectly(const QString& time) {
    _model->removeTimeSlotByString(time);
}

void ScheduleController::editTimeSlotDirectly(int row, const QString& time) {
    _model->updateTimeSlot(row, time);
}

void ScheduleController::editTimeSlotByStringDirectly(const QString& oldTime, const QString& newTime) {
    if (!_model || !_model->getTimesRow()) return;
    const int row = _model->getTimesRow()->indexOf(oldTime);
    if (row >= 0) _model->updateTimeSlot(row, newTime);
}

void ScheduleController::addDateSlotDirectly(const QString& date) {
    int col = _model->columnCount();
    _model->addDateSlot(date, QModelIndex(), col, col);
}

void ScheduleController::removeDateSlotDirectly(const QString& date) {
    _model->removeDateSlotByString(date);
}

void ScheduleController::deleteTimeRow(int logicalIndex) {
    if (!_model || logicalIndex < 0 || logicalIndex >= _model->rowCount()) return;
    QString timeStr = _model->getTimesRow()->at(logicalIndex);
    QMap<QString, bool> savedData = _model->getTimeSlotData(timeStr);
    _mController->getUndoStack()->push(new ScheduleRemoveTimeCommand(_currentMemberId, timeStr, savedData, _mController, this));
}

void ScheduleController::deleteDateColumn(int logicalIndex) {
    if (!_model || logicalIndex < 0 || logicalIndex >= _model->columnCount()) return;
    QString dateStr = _model->getFullDateString(logicalIndex);
    QMap<QString, bool> savedData = _model->getDateSlotData(dateStr);
    _mController->getUndoStack()->push(new ScheduleRemoveDateCommand(_currentMemberId, dateStr, savedData, _mController, this));
}

void ScheduleController::updateColumnDate(int logicalIndex, const QDate& newDate) {
    if (!_model || !_mController || _currentMemberId.isNull() || !newDate.isValid()
        || logicalIndex < 0 || logicalIndex >= _model->columnCount()) return;
    QString oldDateStr = _model->getFullDateString(logicalIndex);
    if (oldDateStr == newDate.toString("yyyy-MM-dd")) return;
    for (int col = 0; col < _model->columnCount(); ++col) {
        if (col != logicalIndex && _model->getFullDateString(col) == newDate.toString("yyyy-MM-dd")) {
            Q_EMIT addNewTimeRowFail(RTypies::Message::DuplicateAdd);
            return;
        }
    }
    _mController->getUndoStack()->push(new ScheduleEditDateCommand(_currentMemberId, oldDateStr, newDate.toString("yyyy-MM-dd"), _mController, this));
}

void ScheduleController::restoreTimeSlotDirectly(const QString& time, const QMap<QString, bool>& data) {
    _model->restoreTimeSlot(time, data); 
}
void ScheduleController::restoreDateSlotDirectly(const QString& date, const QMap<QString, bool>& data) {
    _model->restoreDateSlot(date, data); 
}
void ScheduleController::updateDateSlotDirectly(const QString& oldDate, const QString& newDate) {
    _model->updateDateSlotByString(oldDate, QDate::fromString(newDate, "yyyy-MM-dd"));
}
void ScheduleController::setCellStateDirectly(const QString& date, const QString& time, bool isBusy) {
    _model->setCellStateByString(date, time, isBusy);
}

