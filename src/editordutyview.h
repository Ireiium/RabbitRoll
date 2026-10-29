#pragma once

#include <QObject>
#include "ElaTableView.h"

class EditorDutyView  : public ElaTableView{
	Q_OBJECT

Q_SIGNALS:
	void requestEditRow(int logicalIndex);
	void requestDeleteRow(int logicalIndex);
	void requestDeleteColumn(int logicalIndex);
	void requestUpdateColumnDate(int logicalIndex, QDate newDate);
	void requestDeleteRows(QList<int> rows);
	void requestDeleteColumns(QList<int> columns);
	void requestClearCells(const QModelIndexList& indices);

public:
	EditorDutyView(QWidget* parent = nullptr);
	~EditorDutyView();

	void updateHeightToContents();
	void setModel(QAbstractItemModel* model);
    void ContentChanged();

private:
	void resizeEvent(QResizeEvent* event);

    void VerticalHeaderContextMenu(const QPoint& pos);
    void HorizontalHeaderContextMenu(const QPoint& pos);
    void TableContextMenu(const QPoint& pos);

	void keyPressEvent(QKeyEvent* event);
};

