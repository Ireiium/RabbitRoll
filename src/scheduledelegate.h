#pragma once

#include <QStyledItemDelegate>

class ScheduleDelegate  : public QStyledItemDelegate
{
	Q_OBJECT

public:
	ScheduleDelegate(QObject *parent);
	~ScheduleDelegate();

	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

	//定义格子的建议大小
	QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

