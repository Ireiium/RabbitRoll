#include "memberinfoview.h"
#include <QHeaderView>
#include "memberinfomodel.h"
#include "RTypies.h"
#include "ElaMenu.h"

MemberInfoView::MemberInfoView(QWidget* parent)
    : ElaTableView(parent)
{
    this->verticalHeader()->setHidden(true);
    this->horizontalHeader()->setMinimumSectionSize(150);
    this->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);//自动拉伸
    this->horizontalHeader()->setStretchLastSection(true);//最后自动拉伸
    this->setSelectionBehavior(QAbstractItemView::SelectRows);//单次点击直接选取一整行
    this->setAlternatingRowColors(false);

    this->verticalHeader()->setHidden(false);

    this->verticalHeader()->setMinimumSectionSize(40);
    this->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);

    this->horizontalHeader()->setMinimumSectionSize(150);
    this->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

	this->setSelectionMode(QAbstractItemView::ExtendedSelection);//允许多选
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &ElaTableView::customContextMenuRequested, this, &MemberInfoView::CustomContextMenu);

    this->verticalHeader()->setHidden(false);
    this->verticalHeader()->setMinimumSectionSize(40);
    this->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);

	changeClickMode(RTypies::View::DoubleClick);
}

void MemberInfoView::removeSelectedRows() {
    auto* model = this->model();

    QModelIndexList selectedRows = this->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) return;

    std::sort(selectedRows.begin(), selectedRows.end(), [](const QModelIndex& a, const QModelIndex& b) {
        return a.row() > b.row();
        });

    for (const QModelIndex& index : selectedRows) {
       /* model->removeRow(index.row());*/
		Q_EMIT deleteSelectedRows(index.row());
    }
}

MemberInfoView::~MemberInfoView()
{}

void MemberInfoView::CustomContextMenu(const QPoint& pos) {
    QModelIndexList selectedRows = this->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) return;

    ElaMenu* menu = new ElaMenu(this);
    menu->setMinimumWidth(120);

    QAction* deleteAction = menu->addAction(QString("删除选中的 %1 名成员").arg(selectedRows.size()));

    connect(deleteAction, &QAction::triggered, this, &MemberInfoView::removeSelectedRows);

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void MemberInfoView::changeClickMode(RTypies::View mode)
{
    disconnect(this, &ElaTableView::clicked, this, &MemberInfoView::setClickMode);
    disconnect(this, &ElaTableView::doubleClicked, this, &MemberInfoView::setClickMode);

    if (mode == RTypies::View::DoubleClick)
    {
        connect(this, &ElaTableView::doubleClicked, this, &MemberInfoView::setClickMode);
    }
    else
    {
        connect(this, &ElaTableView::clicked, this, &MemberInfoView::setClickMode);
    }
}

void MemberInfoView::setClickMode(const QModelIndex& index)
{
    if (!index.isValid()) return;

    MemberInfoModel* memModel = qobject_cast<MemberInfoModel*>(this->model());
    if (!memModel) return;

    QVariantMap allDetails = memModel->getMemberAllData(index.row());
    Q_EMIT memberSelected(allDetails, index.row());
}

void MemberInfoView::scrollToRow(int row) {
    if (row < 0 || row >= model()->rowCount()) return;
    QModelIndex index = model()->index(row, 0);
    this->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    this->scrollTo(index, QAbstractItemView::PositionAtCenter);
    this->setFocus();
}


