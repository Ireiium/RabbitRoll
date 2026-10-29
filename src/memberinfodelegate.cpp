#include "memberinfodelegate.h"
#include <QPainter>
#include <ElaTheme.h>

MemberInfoDelegate::MemberInfoDelegate(QObject *parent)
	: QStyledItemDelegate(parent)
{
}

MemberInfoDelegate::~MemberInfoDelegate()
{}

const QColor MemberInfoDelegate::lightColor("#0078d4");
const QColor MemberInfoDelegate::darkColor("#60CDFF");

void MemberInfoDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QStyleOptionViewItem viewOption(option);
    initStyleOption(&viewOption, index);

    if (option.state & QStyle::State_Selected) {
        QStyledItemDelegate::paint(painter, viewOption, index);

        if (index.column() == 0) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            if(ElaTheme::getInstance()->getThemeMode() == ElaThemeType::Light) {
                painter->setBrush(lightColor);
            }
            else {
                painter->setBrush(darkColor);
            }

            QRectF indicatorRect(option.rect.left() + 2, option.rect.top() + 4, 4, option.rect.height() - 8);
            painter->drawRoundedRect(indicatorRect, 2, 2);
            painter->restore();
        }
    }
    else {
        QStyledItemDelegate::paint(painter, viewOption, index);
    }
}

