#include "scheduledelegate.h"
#include <QPainter>

ScheduleDelegate::ScheduleDelegate(QObject *parent)
	: QStyledItemDelegate(parent)
{}

ScheduleDelegate::~ScheduleDelegate()
{}

void ScheduleDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    bool isBusy = index.data(Qt::UserRole).toBool();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    //绘制背景色
    if (isBusy) {
        painter->fillRect(option.rect, QColor(255, 100, 100));//忙碌：红色
        painter->setPen(Qt::NoPen);
        painter->drawRect(option.rect);
    }
    else {
        painter->fillRect(option.rect, Qt::transparent);//空闲：透明
        painter->setPen(Qt::NoPen);
        painter->drawRect(option.rect);
    }

   

    painter->restore();
}

QSize ScheduleDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    return QSize(100, 40);
}

