#pragma once

#include <QObject>
#include <QStyledItemDelegate>

class MemberInfoDelegate  : public QStyledItemDelegate
{
	Q_OBJECT


public:
	MemberInfoDelegate(QObject *parent);
	~MemberInfoDelegate();

	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
	static const QColor lightColor;
	static const QColor darkColor;
};

