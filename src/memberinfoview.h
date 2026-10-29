#pragma once

#include "ElatableView.h"
#include <QVariantMap>
#include "QModelIndex"
#include "RTypies.h"

class MemberInfoView  : public ElaTableView
{
	Q_OBJECT

public:
	explicit MemberInfoView(QWidget* parent = nullptr);
	~MemberInfoView();
	void removeSelectedRows();
	void changeClickMode(RTypies::View mode);
	void scrollToRow(int row);

Q_SIGNALS:
	void memberSelected(const QVariantMap& details, int row);
	void deleteSelectedRows(int row);
	
private Q_SLOTS :
    void CustomContextMenu(const QPoint& pos);
	void setClickMode(const QModelIndex& index);
};

