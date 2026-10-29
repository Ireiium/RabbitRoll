#include "scheduleview.h"
#include <QHeaderView>
#include "ElaMenu.h"
#include <QMenu>
#include <QCursor>
#include <QDate>
#include "ElaCalendarPicker.h"
#include "ScheduleModel.h"
#include <QEvent>
#include <QKeyEvent>

ScheduleView::ScheduleView(QWidget *parent)
	: ElaTableView(parent)
{
    this->setShowGrid(false);
    this->setSelectionMode(QAbstractItemView::ExtendedSelection);

    this->horizontalHeader()->setVisible(true);
    this->verticalHeader()->setVisible(true);

    this->horizontalHeader()->setMinimumSectionSize(50);// 设置列的最小宽度为50像素
    this->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);//自动拉伸
    this->horizontalHeader()->setStretchLastSection(true);//最后自动拉伸

    this->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);//行高固定
    this->verticalHeader()->setDefaultSectionSize(40);
    
    this->setMinimumHeight(300);

    this->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //this->setSelectionMode(QAbstractItemView::NoSelection);

    //允许行列头右键出菜单
    this->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->verticalHeader(), &QHeaderView::customContextMenuRequested, this, &ScheduleView::VerticalHeaderContextMenu);

    this->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &ScheduleView::HorizontalHeaderContextMenu);

    this->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &ScheduleView::customContextMenuRequested, this, &ScheduleView::TableContextMenu);
}

ScheduleView::~ScheduleView()
{}

void ScheduleView::updateViewHeight() {
    int rows = model()->rowCount();
    int headerHeight = horizontalHeader()->height();
    int contentHeight = 0;
    for (int i = 0; i < rows; ++i) {
        contentHeight += rowHeight(i);
    }
    int totalNeededHeight = headerHeight + contentHeight + 5;
    int minHeight = 300;
    int finalHeight = qMax(minHeight, totalNeededHeight);
    this->setFixedHeight(finalHeight);
}

void ScheduleView::VerticalHeaderContextMenu(const QPoint& pos)
{
    int logicalIndex = this->verticalHeader()->logicalIndexAt(pos);
    if (logicalIndex < 0) return;

    QSet<int> selectedRows;
    for (const QModelIndex& idx : this->selectionModel()->selectedIndexes()) {
        selectedRows.insert(idx.row());
    }

    ElaMenu* menu = new ElaMenu(this);
    menu->setMinimumWidth(120);

    if (selectedRows.contains(logicalIndex) && selectedRows.size() > 1) {
        QAction* batchDeleteAction = menu->addAction(QString("删除选中的 %1 个时间段").arg(selectedRows.size()));
        connect(batchDeleteAction, &QAction::triggered, [=]() {
            Q_EMIT requestDeleteRows(selectedRows.values());
            });
    }
    else {
        QAction* editAction = menu->addAction("编辑");
        QAction* deleteAction = menu->addAction("删除该时间段");

        connect(editAction, &QAction::triggered, [=]() {
            Q_EMIT requestEditRow(logicalIndex);
            });
        connect(deleteAction, &QAction::triggered, [=]() {
            Q_EMIT requestDeleteRow(logicalIndex);
            });
    }

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void ScheduleView::HorizontalHeaderContextMenu(const QPoint& pos)
{
    int logicalIndex = this->horizontalHeader()->logicalIndexAt(pos);
    if (logicalIndex < 0) return;

    QSet<int> selectedCols;
    for (const QModelIndex& idx : this->selectionModel()->selectedIndexes()) {
        selectedCols.insert(idx.column());
    }

    ElaMenu* menu = new ElaMenu(this);
    menu->setMinimumWidth(120);

    if (selectedCols.contains(logicalIndex) && selectedCols.size() > 1) {
        QAction* batchDeleteAction = menu->addAction(QString("删除选中的 %1 列").arg(selectedCols.size()));
        connect(batchDeleteAction, &QAction::triggered, [=]() {
            Q_EMIT requestDeleteColumns(selectedCols.values());
            });
    }
    else {
        QAction* editAction = menu->addAction("编辑");
        QAction* deleteAction = menu->addAction("删除该日期");

        connect(editAction, &QAction::triggered, [=]() {
            auto* sModel = qobject_cast<ScheduleModel*>(model());
            QString fullDateStr;
            if (sModel) {
                fullDateStr = sModel->getFullDateString(logicalIndex);
            }

            QDate currentColumnDate = QDate::fromString(fullDateStr, "yyyy-MM-dd");
            if (!currentColumnDate.isValid()) {
                currentColumnDate = QDate::currentDate();
            }

            ElaCalendarPicker* tempPicker = new ElaCalendarPicker(this);
            tempPicker->setWindowFlags(Qt::Popup);
            tempPicker->setSelectedDate(currentColumnDate);

            connect(tempPicker, &ElaCalendarPicker::selectedDateChanged, [=](QDate newDate) {
                Q_EMIT requestUpdateColumnDate(logicalIndex, newDate);
                tempPicker->close();
                tempPicker->deleteLater();
                });

            tempPicker->move(QCursor::pos());
            tempPicker->show();
            tempPicker->click();
            });

        connect(deleteAction, &QAction::triggered, [=]() {
            Q_EMIT requestDeleteColumn(logicalIndex);
            });
    }

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void ScheduleView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete) {
        QModelIndexList selectedIndexes = this->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            Q_EMIT requestClearCells(selectedIndexes);
        }
    }
    else {
        ElaTableView::keyPressEvent(event);
    }
}

void ScheduleView::TableContextMenu(const QPoint& pos) {
    QModelIndexList selectedIndexes = this->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;

    ElaMenu* menu = new ElaMenu(this);
    QAction* clearAction = menu->addAction("清空选中时间段");
    connect(clearAction, &QAction::triggered, [=]() {
        Q_EMIT requestClearCells(selectedIndexes);
        });
    menu->exec(QCursor::pos());
    menu->deleteLater();
}

