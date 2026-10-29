#include "windialog.h"
#include "dialogpropertyedit.h"
#include "dialogtimeselect.h"
#include <QDebug>
#include "ElaToolBar.h"
#include <QVBoxLayout>
#include "ElaPushButton.h"
#include "memberpage.h"
#include "dialog.h"
#include <QHBoxLayout>
#include "ElaMessageBar.h"
#include <QOverload>
#include "componentmanager.h"
#include "dialogblank.h"
#include "dialogbatchadd.h"
#include "projectmanager.h"
#include <QFileDialog>
#include <QMessageBox>
#include <memory>
#include <QStackedLayout>


//控制所有一级对话框的底层、UI逻辑以及对话框的工具栏

WinDialog::WinDialog(QWidget *parent)
	: ElaWindow(parent)
{
    auto* scheduleComp = ComponentManager::instance()->getComponent<ScheduleComponent>();
    _sController = scheduleComp->getController();
    auto* memebrComp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    _mController = memebrComp->getController();
    auto* eidtComp = ComponentManager::instance()->getComponent<EditorDutyComponent>();
    _eController = eidtComp->getController();

    this->setIsFixedSize(true);
    this->setIsStayTop(true);
    setIsNavigationBarEnable(false);
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    this->setWindowModality(Qt::WindowModal);
    this->setWindowIcon(QIcon(""));
    this->hide();

    _toolBar = new ElaToolBar(this);
    _toolBar->setFloatable(false);
    _toolBar->setMovable(false);
    addToolBar(Qt::BottomToolBarArea, _toolBar);

    _proPage = new DialogPropertyEdit(this, this);
    this->addPageNode("", _proPage, ElaIconType::Grid);
    _timePage = new DialogTimeSelect(this, this);
    this->addPageNode("", _timePage, ElaIconType::Grid);
    _blankPage = new DialogBlank(this);
    this->addPageNode("", _blankPage, ElaIconType::Grid);
	_batchAddPage = new DialogBatchAdd(this, this);
    this->addPageNode("", _batchAddPage, ElaIconType::Grid);

    connect(_batchAddPage, &DialogBatchAdd::importFromFileRequested, this, [=]() {

        QString filePath = QFileDialog::getOpenFileName(this, "导入成员数据", "", "RabbitRoll Member (*.rmem)");
        if (filePath.isEmpty()) return;

        auto reply = QMessageBox::warning(this, "警告",
            "导入成员数据将覆盖当前所有的成员信息，确定要继续吗？",
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) return;

        _batchAddPage->setLoadingState(true);

        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(ProjectManager::instance(), &ProjectManager::importMembersFinished, this, [=](bool ok) {
            disconnect(*conn);
            _batchAddPage->setLoadingState(false);

            if (ok) {
                ElaMessageBar::success(ElaMessageBarType::BottomRight, "导入成功", "成员数据已成功导入并覆盖", 2000, this);
                this->closeDialog();
            }
            });

        ProjectManager::instance()->importMembersAsync(filePath);
        });

    connect(_batchAddPage, &DialogBatchAdd::batchAddWithFormatRequested, this, [=](const QStringList& lines, const QStringList& format) {
        if (format.isEmpty()) {
            Q_EMIT batchAddMembersSignal(lines);
        }
        else {
            Q_EMIT batchAddWithFormatSignal(lines, format);
        }
        });

    connect(_mController, &MemberInfoController::batchWorkFinished, this, [=]() {
        if (this->isVisible() && _batchAddPage->isVisible()) {
			_batchAddPage->reset();
            this->close();
        }
        });

    connect(_mController, &MemberInfoController::propertyListChanged, this, [=]() {
        if (this->isVisible() && this->getCurrentNavigationPageKey() == _proPage->property("ElaPageKey").toString()) {
            Q_EMIT showProPage();
        }
        });
}

WinDialog::~WinDialog()
{
}



void WinDialog::openWinDialog(const RTypies::WinDialog key, QVariant data) {
    _key = key;
    if (key == RTypies::WinDialog::EditProperties) {
        showPropertyEditDialog();
    }
    else if (key == RTypies::WinDialog::AddMemberTimeRow || key == RTypies::WinDialog::AddEditorTimeRow) {
        _editIndex = -1;
        _timePage->setTime(0, 0, 0, 0);
        showSelectTimeDialog();
    }
    else if (key == RTypies::WinDialog::EditMemberTimeRow || key == RTypies::WinDialog::EditEditorTimeRow) {
        if (data.canConvert<QVariantList>()) {
            QVariantList params = data.toList();
            if (params.size() >= 5) {
                _editIndex = params[0].toInt();
                _timePage->setTime(params[1].toInt(), params[2].toInt(), params[3].toInt(), params[4].toInt());
            }
        }
        else if (data.canConvert<QList<int>>()) {
            QList<int> params = data.value<QList<int>>();
            if (params.size() >= 5) {
                _editIndex = params[0];
                _timePage->setTime(params[1], params[2], params[3], params[4]);
            }
        }
        showSelectTimeDialog();
    }
    else if (key == RTypies::WinDialog::CreateNew) {
        showCreatNewDialog();
	}
    else if (key == RTypies::WinDialog::BatchAddMembers) {
        showBatchAddDialog();
    }
}

void WinDialog::showPropertyEditDialog() {
    this->navigation(_proPage->property("ElaPageKey").toString());
    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    resize(600, 400);
    setWindowTitle("属性设置");

    _toolBar->setFixedSize(600, 55);
    _toolBar->setVisible(true);
    _toolBar->clear();//清空工具栏上的按钮

    ElaPushButton* selcet = new ElaPushButton("全部选择", _toolBar);
    selcet->resize(150, 40);
    _toolBar->addWidget(selcet);

    _toolBar->addSeparator();

    ElaPushButton* addBtn = new ElaPushButton("添加", _toolBar);
    addBtn->resize(100, 40);
    _toolBar->addWidget(addBtn);

    ElaPushButton* delBtn = new ElaPushButton("删除", _toolBar);
    delBtn->resize(100, 40);
    _toolBar->addWidget(delBtn);

    Q_EMIT showProPage();//发给propertyeditdialo，打开展示页（这样操作可以让弹窗有一个白色背景，原理同mainwindow的页面展示）

    connect(addBtn, &ElaPushButton::clicked, this, [=]() {
        Dialog* dialog = new Dialog(RTypies::Dialog::AddProperties, this);
        dialog->show();
        connect(dialog, &Dialog::certainClicked_newProperty, this, [=](QString newAttr) {
            int isSucceed = _mController->addNewProperty(newAttr);
            switch (isSucceed) {
            case 0:
                Q_EMIT showProPage();//刷新
                Q_EMIT actionSuccess(RTypies::Page::UpdateShowPropertyList, QVariant());
                ElaMessageBar::success(ElaMessageBarType::BottomRight, "成功添加标题", "", 1000, this);
                break;
            case 1:
                ElaMessageBar::error(ElaMessageBarType::BottomRight, "添加失败", "属性已存在", 1000, this);
                break;
            case 2:
                ElaMessageBar::error(ElaMessageBarType::BottomRight, "添加失败", "属性名不能为空", 1000, this);
                break;
            case 3:
                ElaMessageBar::error(ElaMessageBarType::BottomRight, "添加失败", "属性名长度不能大于8", 1000, this);
                break;
            default:
                qDebug() << "添加属性失败，未知错误:" << newAttr;
                ElaMessageBar::error(ElaMessageBarType::BottomRight, "添加失败", "未知错误", 1000, this);
            }
            });
        });


    connect(delBtn, &ElaPushButton::clicked, this, [=]() {
        Q_EMIT deleteChecked();
        Q_EMIT actionSuccess(RTypies::Page::UpdateShowPropertyList, QVariant());
        });

    connect(selcet, &ElaPushButton::clicked, this, [=]() {
        if (selcet->text() == "全部选择") {
            Q_EMIT selectAll();
            selcet->setText("取消选择");
        }
        else {
            Q_EMIT unSelectAll();
            selcet->setText("全部选择");
        }
        });

    this->show();
}

void WinDialog::showSelectTimeDialog() {
    this->navigation(_timePage->property("ElaPageKey").toString());

    setMaximumSize(600, 400);
    resize(600, 400);
    _toolBar->setVisible(false);
    if (_editIndex == -1) {
        setWindowTitle("添加时间段");
    }
    else {
        setWindowTitle("编辑时间段");
    }

    this->show();
}

void  WinDialog::showCreatNewDialog() {

    setMaximumSize(600, 400);
    resize(600, 400);
    _toolBar->setVisible(false);
    setWindowTitle("新建");

    this->show();
}


void WinDialog::timeSelectCertainNews(const int hour1, const int minute1, const int hour2, const int minute2) {
    if (_key == RTypies::WinDialog::AddMemberTimeRow) {
        _sController->addTimeSlot(hour1, minute1, hour2, minute2);
    }
    else if (_key == RTypies::WinDialog::AddEditorTimeRow) {
        _eController->addTimeRow(hour1, minute1, hour2, minute2);
    }
    else if (_key == RTypies::WinDialog::EditMemberTimeRow) {
        _sController->editTimeSlot(_editIndex, hour1, minute1, hour2, minute2);
    }
    else if (_key == RTypies::WinDialog::EditEditorTimeRow) {
        _eController->editTimeRow(_editIndex, hour1, minute1, hour2, minute2);
    }
}


void WinDialog::showBatchAddDialog() {

    QString currentKey = this->getCurrentNavigationPageKey();
    QString targetKey = _batchAddPage->property("ElaPageKey").toString();

    if (currentKey == targetKey) {
        if (_batchAddPage->_stackedLayout->currentIndex() == 1) {
            return;
        }
    }
    this->navigation(_batchAddPage->property("ElaPageKey").toString());
    setMaximumSize(600, 400);
    resize(600, 400);
    _toolBar->setVisible(false);
    setWindowTitle("批量添加");
	_batchAddPage->reset();
    this->show();
}

void WinDialog::batchAddMembers(const QStringList& members) {
	Q_EMIT batchAddMembersSignal(members);
}

void WinDialog::closeDialog() {
    this->close();
}

void WinDialog::cleanAndDeleteToorBar() {
    _toolBar->clear();
}

void WinDialog::hideEvent(QHideEvent* event)
{
    this->navigation(_blankPage->property("ElaPageKey").toString());
    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    cleanAndDeleteToorBar();
    ElaWindow::hideEvent(event);
}
