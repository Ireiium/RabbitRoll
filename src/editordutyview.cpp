#include "editordutyview.h"
#include <QHeaderView>
#include <QResizeEvent>
#include "ElaMenu.h"
#include <QDate>
#include "editordutymodel.h"
#include "ElaCalendarPicker.h"

EditorDutyView::EditorDutyView(QWidget* parent)
    : ElaTableView(parent)
{
    this->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    //允许滚动
    this->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    this->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->setAlternatingRowColors(false);
    /*this->setShowGrid(true);*/

    this->horizontalHeader()->setStretchLastSection(false);

    this->horizontalHeader()->setMinimumSectionSize(80);
    this->verticalHeader()->setMinimumSectionSize(40);

    //this->setCornerButtonEnabled(false);
    //允许行列头右键出菜单
    this->verticalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->verticalHeader(), &QHeaderView::customContextMenuRequested, this, &EditorDutyView::VerticalHeaderContextMenu);

    this->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &EditorDutyView::HorizontalHeaderContextMenu);

    this->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &EditorDutyView::customContextMenuRequested, this, &EditorDutyView::TableContextMenu);
}
EditorDutyView::~EditorDutyView() {

}

void EditorDutyView::setModel(QAbstractItemModel* model) {
    ElaTableView::setModel(model);
    if (model) {
        connect(model, &QAbstractItemModel::dataChanged, this, &EditorDutyView::ContentChanged);
        connect(model, &QAbstractItemModel::modelReset, this, &EditorDutyView::ContentChanged);
        connect(model, &QAbstractItemModel::rowsInserted, this, &EditorDutyView::ContentChanged);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &EditorDutyView::ContentChanged);
        connect(model, &QAbstractItemModel::columnsInserted, this, &EditorDutyView::ContentChanged);
        connect(model, &QAbstractItemModel::columnsRemoved, this, &EditorDutyView::ContentChanged);

        ContentChanged();
    }
}

void EditorDutyView::keyPressEvent(QKeyEvent* event) {
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

void EditorDutyView::ContentChanged() {
    if (!model()) return;

    QHeaderView* hHeader = this->horizontalHeader();
    int columnCount = model()->columnCount();
    hHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->resizeColumnsToContents();
    this->resizeRowsToContents();

    int totalContentWidth = 0;
    for (int i = 0; i < columnCount; ++i) {
        totalContentWidth += columnWidth(i);
    }

    int availableWidth = this->viewport()->width();

    if (columnCount > 0 && columnCount <= 7 && totalContentWidth < availableWidth) {
        hHeader->setSectionResizeMode(QHeaderView::Stretch);
    }
    else {
        hHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
    }

    updateHeightToContents();
}

void EditorDutyView::updateHeightToContents() {
    if (!model()) return;

    int totalHeight = 0;
    if (horizontalHeader()->isVisible()) {
        totalHeight += horizontalHeader()->height();
    }
    int rowCount = model()->rowCount();
    for (int i = 0; i < rowCount; ++i) {
        totalHeight += rowHeight(i);
    }
    totalHeight += frameWidth() * 2 + 2;

    this->setMinimumHeight(qMax(300, totalHeight));
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void EditorDutyView::resizeEvent(QResizeEvent* event) {
    ElaTableView::resizeEvent(event);
    if (event->size().width() != event->oldSize().width()) {
        ContentChanged();
    }
}

void EditorDutyView::VerticalHeaderContextMenu(const QPoint& pos)
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
            this->updateHeightToContents();
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
            this->updateHeightToContents();
            });
    }

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void EditorDutyView::HorizontalHeaderContextMenu(const QPoint& pos)
{
    int logicalIndex = this->horizontalHeader()->logicalIndexAt(pos);
    if (logicalIndex < 0) return;

    QSet<int> selectedCols;
    for (const QModelIndex& idx : this->selectionModel()->selectedIndexes()) {
        selectedCols.insert(idx.column());
    }

    ElaMenu* menu = new ElaMenu(this);
    menu->setMinimumWidth(120);
    auto* eModel = qobject_cast<EditorDutyModel*>(model());


    if (selectedCols.contains(logicalIndex) && selectedCols.size() > 1) {
        QAction* batchDeleteAction = menu->addAction(QString("删除选中的 %1 列").arg(selectedCols.size()));
        connect(batchDeleteAction, &QAction::triggered, [=]() {
            Q_EMIT requestDeleteColumns(selectedCols.values());
            this->updateHeightToContents();
            });
    }
    else {

        QAction* editAction = menu->addAction("编辑");
        QAction* deleteAction = menu->addAction("删除该日期");

        if (eModel && eModel->getColumnMode() == EditorDutyModel::WeekMode) {
            editAction->setVisible(false);
        }

        connect(editAction, &QAction::triggered, [=]() {
            QString fullDateStr;
            if (eModel) {
                fullDateStr = eModel->getFullDateString(logicalIndex);
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
            this->updateHeightToContents();
            });
    }

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void EditorDutyView::TableContextMenu(const QPoint& pos) {
    QModelIndexList selectedIndexes = this->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;

    ElaMenu* menu = new ElaMenu(this);
    QAction* clearAction = menu->addAction("清空内容");
    connect(clearAction, &QAction::triggered, [=]() {
        Q_EMIT requestClearCells(selectedIndexes);
        });
    menu->exec(QCursor::pos());
    menu->deleteLater();
}
