#include "memberpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include "ElaPushButton.h"
#include "ElaMultiSelectComboBox.h"
#include "MemberInformationDock.h"
#include "ElaText.h"
#include "ElaWindow.h"
#include "windialog.h"
#include "ElaMessageBar.h"
#include "componentmanager.h"
#include "projectmanager.h"
#include <QFileDialog>
#include "scriptengine.h"
#include "ElaSuggestBox.h"

MemberPage::MemberPage(QWidget* parent)
	: ElaScrollPage(parent)
{
    auto* mComp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    _controller = mComp->getController();
    auto* sComp = ComponentManager::instance()->getComponent<ScheduleComponent>();
    _sController = sComp->getController();

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setWindowTitle("成员信息");

    _infoDock = new MemberInformationDock(this);
    _infoDock->hide();

    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QHBoxLayout* contentLayout = new QHBoxLayout();

    ElaText* proLable = new ElaText("属性:");
    buttonLayout->addWidget(proLable);
    proLable->setTextPixelSize(18);
    _showProBtn = new ElaMultiSelectComboBox();
    _showProBtn->resize(120, 58);
    _showProBtn->setEditable(false);
    showAddItem(_controller->getAllPropertyNames(), _showProBtn);
    _showProBtn->setCurrentSelection(0);//默认显示“姓名”这一列
    buttonLayout->addWidget(_showProBtn);
    buttonLayout->addSpacing(10);
    ElaPushButton* addBtn = new ElaPushButton("添加成员");
    addBtn->resize(120, 60);
    buttonLayout->addWidget(addBtn);
    buttonLayout->addSpacing(10);
	ElaPushButton* deleteBtn = new ElaPushButton("删除成员");
	deleteBtn->resize(120, 60);
	buttonLayout->addWidget(deleteBtn);
    buttonLayout->addSpacing(10);
	ElaPushButton* setProyBtn = new ElaPushButton("管理属性");
    setProyBtn->resize(120, 60);
	buttonLayout->addWidget(setProyBtn);
    buttonLayout->addSpacing(10);  
    ElaPushButton* batchAddBtn = new ElaPushButton("批量添加");
    batchAddBtn->resize(120, 60);
    buttonLayout->addWidget(batchAddBtn);
    buttonLayout->addSpacing(10);
    ElaPushButton* exportBtn = new ElaPushButton("导出");
    exportBtn->resize(120, 60);
    buttonLayout->addWidget(exportBtn);
    buttonLayout->addSpacing(10);
	_recoverBtn = new ElaPushButton("撤销脚本修改");
    _recoverBtn->setFixedSize(120, 38);
	setRecoveryBtn(false);
    buttonLayout->addWidget(_recoverBtn);
    buttonLayout->addStretch();
    _searchBox = new ElaSuggestBox(this);
    _searchBox->setFixedWidth(180);
    _searchBox->setPlaceholderText("搜索成员姓名...");
	buttonLayout->addWidget(_searchBox);

    connect(_searchBox, &ElaSuggestBox::suggestionClicked, this, [=](QString suggestText, QVariantMap suggestData) {
        int row = suggestData.value("row").toInt();
        _controller->scrollToRow(row);
        });
    connect(_controller, &MemberInfoController::searchDataReady, this, &MemberPage::onSearchDataReady);
    _controller->requestSearchSuggestionsUpdate();

    connect(_recoverBtn, &ElaPushButton::clicked, this, &MemberPage::onRecoverBtnClicked);

    connect(exportBtn, &ElaPushButton::clicked, this, [=]() {
        QString filePath = QFileDialog::getSaveFileName(this, "导出成员列表", "RabbitRoll 成员", "RabbitRoll Member (*.rmem)");
        if (filePath.isEmpty()) return;

        ProjectManager::instance()->exportMembersAs(filePath);
        });

    //监听导出结果
    connect(ProjectManager::instance(), &ProjectManager::exportFinished, this, [=](bool success) {
        if (success) {
            QWidget* parent = this->window() ? this->window() : this;
            ElaMessageBar::success(ElaMessageBarType::BottomLeft, "导出成功", "成员数据已成功保存", 2000, parent);
        }
        });

    _controller->setupView(contentLayout);
    layout->addLayout(buttonLayout);
    layout->addLayout(contentLayout);
    this->addCentralWidget(centralWidget);

   connect(batchAddBtn,&ElaPushButton::clicked, this, [=](){
        Q_EMIT popWinDialog(RTypies::WinDialog::BatchAddMembers, QVariant());
	   });

    connect(_showProBtn, &ElaMultiSelectComboBox::currentTextListChanged, this, [=](const QStringList& selectedList) {
        if (!selectedList.contains("姓名")) {
            QStringList fixedList = selectedList;
            fixedList.prepend("姓名");
            _showProBtn->blockSignals(true);
            _showProBtn->setCurrentSelection(fixedList);
            _showProBtn->blockSignals(false);
        }
        QStringList finalSelected = _showProBtn->getCurrentSelection();
        _controller->updatePropertyVisibility(finalSelected);
        });


    connect(addBtn, &ElaPushButton::clicked, this, [=](){
        _controller->addMember("新成员");
        });

    connect(deleteBtn, &ElaPushButton::clicked, this, [=]() {
        _controller->removeSelectedMembers();
        });
    
    //属性设置
    connect(setProyBtn, &ElaPushButton::clicked, this, [=]() {
        Q_EMIT popWinDialog(RTypies::WinDialog::EditProperties, QVariant());
        });

    //中转时间选取
    connect(_infoDock, &MemberInformationDock::popWinDialog, this, [=](const RTypies::WinDialog key) {
        Q_EMIT popWinDialog(RTypies::WinDialog::AddMemberTimeRow, QVariant());
        });

    //收到dock传来的属性更新指令并处理
    connect(_infoDock, &MemberInformationDock::propertyChanged, this, [=](QString attrName, QString newValue) {
        int row = _controller->getCurrentRow();
        if (row >= 0) {
            _controller->updateMemberProperty(row, attrName, newValue);
        }
        });

    connect(_infoDock, &MemberInformationDock::noteChanged, this, [=](int row, QString newNote) {
        if (row >= 0) {
            /*qDebug() << "保存note";*/
            _controller->updateMemberNote(row, newNote);
        }
        });

    connect(_infoDock, &MemberInformationDock::exportSchedule,this, &MemberPage::exportSchedule);
    connect(_infoDock, &MemberInformationDock::finishImportSchedule, this, [=](bool ok) {
        QWidget* parent = this->window() ? this->window() : this;
        if (ok) {
            ElaMessageBar::success(ElaMessageBarType::BottomLeft, "导入成功", "数据导入成功", 2000, parent);
        }
        else {
			ElaMessageBar::error(ElaMessageBarType::BottomLeft, "导入失败", "数据导入失败", 2000, parent);
        }
        });
    connect(_infoDock, &MemberInformationDock::finishExportSchedule, this, [=](bool ok) {
        QWidget* parent = this->window() ? this->window() : this;
        if (ok) {
            ElaMessageBar::success(ElaMessageBarType::BottomLeft, "成功", "数据导出成功", 2000, parent);
        }
        else {
            ElaMessageBar::error(ElaMessageBarType::BottomLeft, "失败", "数据导出失败", 2000, parent);
        }
        });
    
    //打开dock
    connect(_controller, &MemberInfoController::memberSelected, this, [=](const QVariantMap details,int row) {
        _infoDock->setMemberDetails(details,row);
        _curSeeRow = row;
        showInfoDock();
        });

    connect(ProjectManager::instance(), &ProjectManager::projectLoaded, this, [=]() {
        QStringList allProps = _controller->getAllPropertyNames();
        showAddItem(allProps, _showProBtn);

        if (_infoDock) _infoDock->hide();
        });

    //监听属性变化
    connect(_controller, &MemberInfoController::propertyListChanged, this, [=]() {
        QStringList allProps = _controller->getAllPropertyNames();
        showAddItem(allProps, _showProBtn);
        });

    connect(_sController, &ScheduleController::addNewTimeRowFail, this, [=](RTypies::Message key) {
        QWidget* parent = this->window() ? this->window() : this;
        switch (key)
        {
        case RTypies::Message::TimeIsNotValid:
            ElaMessageBar::error(ElaMessageBarType::BottomLeft, "添加失败", "时间不合法", 1000, parent);
            break;
        case RTypies::Message::DuplicateAdd:
            Q_EMIT ElaMessageBar::error(ElaMessageBarType::BottomLeft, "添加失败", "时间重复", 1000, parent);
            break;
        default:
            break;
        }
        });

    //触发编辑弹窗
    connect(_sController, &ScheduleController::popWinDialogRequest, this, [=](const RTypies::WinDialog key, QVariant data) {
        Q_EMIT popWinDialog(key, data);
        });

    connect(ScriptEngine::instance(), &ScriptEngine::scriptChangedData, this, &MemberPage::setRecoveryBtn);

}

void MemberPage::hideEvent(QHideEvent* event) {
    //_infoDock && !_infoDock->isFloating(
    if (_infoDock) {
        _curSeeRow = -1;
        _infoDock->hide();
    }
    ElaScrollPage::hideEvent(event);
}

void MemberPage::showInfoDock() {
    if (_infoDock) {
        ElaWindow* win = qobject_cast<ElaWindow*>(this->window());
        if (win) {
            Qt::DockWidgetArea currentArea = win->dockWidgetArea(_infoDock);
            if (currentArea == Qt::NoDockWidgetArea || currentArea == Qt::AllDockWidgetAreas) {
                currentArea = Qt::RightDockWidgetArea;//默认给右边
            }
            win->addDockWidget(currentArea, _infoDock);
            _infoDock->show();
            _infoDock->raise();
        }
    }
}


//旧，现在尽量不走这个转发
//暴露给mainwindow（管理器）的接口，用于链接其他page（如dialog）传给自己的数据
void MemberPage::updatePage(RTypies::Page key, QVariant data) {
    switch (key) {
    case RTypies::Page::UpdateShowPropertyList:
        showAddItem(_controller->getAllPropertyNames(), _showProBtn);
        break;
    /*case RTypies::Page::UpdateInfoDockTimeRow:
        timeSelectCertain(data.toList().at(0).toInt(), data.toList().at(1).toInt(), data.toList().at(2).toInt(), data.toList().at(3).toInt());
        break;*/
    default:
        break;
    }
}

void MemberPage::showAddItem(const QList<QString>& items, ElaMultiSelectComboBox* box) {
    if (!box || !_controller) return;

    box->blockSignals(true);
    box->clear();
    box->addItems(items);

    QStringList selectedList;
    QVariantMap showMap = _controller->getPropertyVisibilityMap();
    for (const QString& item : items) {
        if (item == "姓名" || showMap.value(item, false).toBool()) {
            selectedList << item;
        }
    }
    box->setCurrentSelection(selectedList);
    box->blockSignals(false);
}

void MemberPage::expoertSucess() {
    ElaMessageBar::success(ElaMessageBarType::BottomLeft, "导出成功", "成员数据已成功保存", 2000, this);
}

void MemberPage::onRecoverBtnClicked() {
    if (!_controller) return;

    QUndoStack* stack = _controller->getUndoStack();

    if (stack->undoText() == "PythonChangedData") {
        stack->undo();
    }

    updateRecoverButtonState();
}

void MemberPage::setRecoveryBtn(bool isChanged) {
    if (!_recoverBtn) return;
    _isScriptChangedData = isChanged;

    if (isChanged) {
        _recoverBtn->show();
        _recoverBtn->setEnabled(true);
    }
    else {
        _recoverBtn->hide();
    }
}

void MemberPage::updateRecoverButtonState() {
    if (_controller->getUndoStack()->undoText() != "PythonChangedData") {
        setRecoveryBtn(false);
    }
}

void MemberPage::onSearchDataReady(const QStringList& names, const QList<QVariantMap>& dataList) {
    if (!_searchBox) return;

    for (const QString& key : _currentSuggestKeys) {
        _searchBox->removeSuggestion(key);
    }
    _currentSuggestKeys.clear();
    for (int i = 0; i < names.size(); ++i) {
        QString key = _searchBox->addSuggestion(names[i], dataList[i]);
        _currentSuggestKeys.append(key);
    }
}


MemberPage::~MemberPage()
{

}
