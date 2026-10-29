#pragma once

#include "ElaTableView.h"

class QDate;
class ScheduleView  : public ElaTableView
{
	Q_OBJECT

friend class ScheduleController;
friend class ScheduleComponent;

Q_SIGNALS:
	void requestDeleteRow(int logicalIndex);
	void requestDeleteColumn(int logicalIndex);
	void requestEditRow(int index);
	void requestUpdateColumnDate(int logicalIndex, QDate newDate);
	void getFullDateString(int column);
	void requestDeleteRows(QList<int> rows);
	void requestDeleteColumns(QList<int> columns);
	void requestClearCells(QList<QModelIndex> cells);

public:
	ScheduleView(QWidget* parent = nullptr);
	~ScheduleView();

	void updateViewHeight();

    void VerticalHeaderContextMenu(const QPoint& pos);
    void HorizontalHeaderContextMenu(const QPoint& pos);
    void TableContextMenu(const QPoint& pos);

protected:
	void keyPressEvent(QKeyEvent* event) override;

private:
	QString _fullDateStr{ "" };
};

