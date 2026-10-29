#include "editpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "componentmanager.h"
#include "ElaPushButton.h"
#include "idlematchservice.h"
#include "editorselectDock.h"
#include "ElaWindow.h"
#include "ElaMessageBar.h"
#include <QDialog>
#include <QSpinBox>
#include <QCheckBox>
#include "ElaText.h"
#include "ElaCheckBox.h"
#include "ElaSpinBox.h"
#include "dialogaction.h"
#include "ElaProgressRing.h"
#include <QDateEdit>
#include <QMessageBox>
#include <QKeyEvent>
#include <functional>
#include "settings.h"
#include "ElaCalendarPicker.h"
#include <QDir>
#include "rconsole.h"
#include <QStackedWidget>
#include "AIServiceInterface.h"
#include "ElaScrollArea.h"
#include <QApplication>
#include "ElaScrollBar.h"
#include <QScrollBar>

EditPage::EditPage(QWidget *parent)
	: ElaScrollPage(parent)
{
	initialPtr();
	initialUI();
	initialSignalAndSlot();
	resetBtnText();
}

EditPage::~EditPage()
{}

void EditPage::initialPtr() {
	auto* eComp = ComponentManager::instance()->getComponent<EditorDutyComponent>();
	_controller = eComp->getController();

	_selectDock = new EditorSelectDock(this);
	_selectDock->hide();

	_changeModeBtn = new ElaPushButton("切换模式", this);
	_addTimeRowBtn = new ElaPushButton("添加时间", this);
	_addDateColBtn = new ElaPushButton("添加日期", this);
	_scheduleBtn = new ElaPushButton("一键排班", this);
	_AIBtn = new ElaPushButton("AI排班", this);
}

void EditPage::initialUI() {
	QWidget* container = new QWidget(this);
	container->setWindowTitle("编辑");
	this->addCentralWidget(container);
	QVBoxLayout* mainLayout = new QVBoxLayout(container);
	QHBoxLayout* btnLayout = new QHBoxLayout();
	mainLayout->addLayout(btnLayout);

	_controller->setupView(mainLayout);

	_changeModeBtn->setFixedSize(120, 38);
	btnLayout->addWidget(_changeModeBtn);
	_addTimeRowBtn->resize(120, 60);
	btnLayout->addWidget(_addTimeRowBtn);
	_addDateColBtn->resize(120, 60);
	btnLayout->addWidget(_addDateColBtn); 
	_scheduleBtn->resize(120, 60);
	btnLayout->addWidget(_scheduleBtn);
	btnLayout->addWidget(_AIBtn);
	btnLayout->addStretch();

	mainLayout->addStretch();


}

void EditPage::initialSignalAndSlot() {
	connect(_addTimeRowBtn, &ElaPushButton::clicked, [=]() {
		Q_EMIT popWinDialog(RTypies::WinDialog::AddEditorTimeRow, QVariant());
		});

	connect(_addDateColBtn, &ElaPushButton::clicked, [=]() {
		_controller->addColumn();
		});

	connect(_changeModeBtn, &ElaPushButton::clicked, [=]() {
		_controller->switchMode();
		resetBtnText();
		});

	connect(_scheduleBtn, &ElaPushButton::clicked, this, [=]() {
		popScheduleWindow();
		});

	connect(_AIBtn, &ElaPushButton::clicked, this, [=]() {
#ifdef ENABLE_AI_SUPPORT
		if (_isAIMode) {
			popAIDialog();
		}
#endif
		});

	connect(IdleMatchService::instance(), &IdleMatchService::precalculationFinished, _selectDock, &EditorSelectDock::onCalculationFinished);

	connect(_controller, &EditorDutyController::cellDoubleClicked, this, &EditPage::onCellDoubleClicked);

	connect(_controller, &EditorDutyController::addTimeError, this, [=](RTypies::Message key) {
        QWidget* parent = this->window() ? this->window() : this;
		switch (key)
		{
		case RTypies::Message::TimeIsNotValid:
            ElaMessageBar::error(ElaMessageBarType::BottomLeft, "添加失败", "时间不合法", 1000, parent);
			break;
		case RTypies::Message::DuplicateAdd:
            ElaMessageBar::error(ElaMessageBarType::BottomLeft, "添加失败", "时间重复", 1000, parent);
			break;
		default:
			break;
		}
		});

	connect(_controller, &EditorDutyController::popWinDialogRequest, this, [=](const RTypies::WinDialog key, QVariant data) {
		Q_EMIT popWinDialog(key, data);
		});

	connect(_controller, &EditorDutyController::scheduleFinish, this, [=](RTypies::Message msg, QString errorMsg) {
		bool consoleEnabled = Settings::instance()->getEnableConsole();

        QWidget* parent = this->window() ? this->window() : this;
        if (msg == RTypies::Message::Success) {
			if (!errorMsg.isEmpty() && (errorMsg.contains("出错") || errorMsg.contains("异常") || errorMsg.contains("未定义"))) {
                ElaMessageBar::error(ElaMessageBarType::BottomLeft, "排班异常", "脚本执行出现错误，请检查", 3000, parent);
				if (consoleEnabled) RConsole::instance()->appendLog("[排班异常中断] " + errorMsg);
			}
			else {
                ElaMessageBar::success(ElaMessageBarType::BottomLeft, "排班完成", "排班结果已生成", 1500, parent);
				if (consoleEnabled) RConsole::instance()->appendLog("[排班成功] 结果已成功写入视图。");
			}
		}
		else if (msg == RTypies::Message::Failure) {
			if (errorMsg.isEmpty()) {
                ElaMessageBar::error(ElaMessageBarType::BottomLeft, "排班失败", "发生未知错误，请重试", 3000, parent);
				if (consoleEnabled) RConsole::instance()->appendLog("[排班失败] 未知错误");
			}
			else {
                ElaMessageBar::error(ElaMessageBarType::BottomLeft, "排班失败", errorMsg, 3000, parent);
				if (consoleEnabled) RConsole::instance()->appendLog("[排班失败] " + errorMsg);
			}
		}
		});
}

void EditPage::resetBtnText() {
	if (_controller->getMode()== 0) {
		_changeModeBtn->setText("切换到日期模式");
		_addDateColBtn->setText("添加星期");
	}
	else {
		_changeModeBtn->setText("切换到周期模式");
		_addDateColBtn->setText("添加日期");
	}
}


void EditPage::onCellDoubleClicked(int row, int col) {
	QModelIndex index = _controller->getIndex(row, col);

	if (!index.isValid()) return;

	bool busy = IdleMatchService::instance()->isCalculating();

	_selectDock->setTargetCell(index, busy);

	showInfoDock();
}

void EditPage::showInfoDock() {
	if (_selectDock) {
		ElaWindow* win = qobject_cast<ElaWindow*>(this->window());
		if (win) {
			Qt::DockWidgetArea currentArea = win->dockWidgetArea(_selectDock);
			if (currentArea == Qt::NoDockWidgetArea || currentArea == Qt::AllDockWidgetAreas) {
				currentArea = Qt::RightDockWidgetArea;//默认给右边
			}
			win->addDockWidget(currentArea, _selectDock);
			_selectDock->show();
			_selectDock->raise();
		}
	}
}

void EditPage::popScheduleWindow() {
	DialogAction dialog(this);
	dialog.setWindowTitle("一键排班设置");

	AIDialogCloseFilter* closeFilter = new AIDialogCloseFilter(&dialog);
	dialog.installEventFilter(closeFilter);

	closeFilter->onAbort = [this]() {
		qDebug() << "用户强制中止了排班计算";
		_controller->abortSchedule();
		};

	QVBoxLayout* mainLayout = new QVBoxLayout();
	QWidget* formWidget = new QWidget(&dialog);
	QVBoxLayout* contentLayout = new QVBoxLayout(formWidget);
	contentLayout->setAlignment(Qt::AlignHCenter);

	QHBoxLayout* countLayout = new QHBoxLayout();
	countLayout->addWidget(new ElaText("单次排班人数:", 14));
	ElaSpinBox* countSpin = new ElaSpinBox();
	countSpin->setRange(1, 10);
	countSpin->setValue(2);
	countSpin->setFixedSize(100, 30);
	countLayout->addWidget(countSpin);
	contentLayout->addLayout(countLayout);
	contentLayout->addSpacing(15);

	ElaCheckBox* priorityCheck = new ElaCheckBox("优先选择排班次数少的成员");
	priorityCheck->setChecked(true);
	contentLayout->addWidget(priorityCheck, 0, Qt::AlignHCenter);
	contentLayout->addSpacing(10);

	ElaCheckBox* multiCheck = new ElaCheckBox("允许同一成员一周多次排班");
	multiCheck->setChecked(false);
	if (_controller->getMode() != 0) multiCheck->setVisible(false);
	contentLayout->addWidget(multiCheck, 0, Qt::AlignHCenter);
	contentLayout->addSpacing(10);

	ElaCheckBox* scriptCheck = new ElaCheckBox("是否使用Python脚本排班 ");
	scriptCheck->setChecked(false);
	if (!_isScriptMode || _script.isEmpty() || _script == "未找到脚本文件") {
		scriptCheck->setDisabled(true);
	}
	contentLayout->addWidget(scriptCheck, 0, Qt::AlignHCenter);
	contentLayout->addStretch();

	QWidget* progressWidget = new QWidget(&dialog);
	QVBoxLayout* progressLayout = new QVBoxLayout(progressWidget);
	progressLayout->setAlignment(Qt::AlignCenter);

	ElaProgressRing* progressRing = new ElaProgressRing(&dialog);
	progressRing->setIsBusying(true);
	progressRing->setFixedSize(40, 40);
	ElaText* progressText = new ElaText("正在计算排班，请稍候...", 13);

	progressLayout->addWidget(progressRing, 0, Qt::AlignHCenter);
	progressLayout->addSpacing(15);
	progressLayout->addWidget(progressText, 0, Qt::AlignHCenter);
	progressWidget->hide();

	QWidget* btnWidget = new QWidget(&dialog);
	QHBoxLayout* btnLayout = new QHBoxLayout(btnWidget);
	btnLayout->addStretch();
	ElaPushButton* okBtn = new ElaPushButton("确定");
	okBtn->setFixedSize(80, 40);
	ElaPushButton* cancelBtn = new ElaPushButton("取消");
	cancelBtn->setFixedSize(80, 40);
	btnLayout->addWidget(okBtn);
	btnLayout->addSpacing(15);
	btnLayout->addWidget(cancelBtn);
	btnLayout->addStretch();

	mainLayout->addWidget(formWidget);
	mainLayout->addWidget(progressWidget);
	mainLayout->addWidget(btnWidget);

	connect(scriptCheck, &ElaCheckBox::toggled, this, [=](bool checked) {
		priorityCheck->setEnabled(!checked);
		priorityCheck->setChecked(false);
		multiCheck->setEnabled(!checked);
		multiCheck->setChecked(false);
		});

	connect(cancelBtn, &ElaPushButton::clicked, &dialog, &QDialog::reject);

	connect(okBtn, &ElaPushButton::clicked, this, [=, &dialog]() {
		formWidget->hide();
		btnWidget->hide();
		progressRing->setIsBusying(true);
		progressWidget->show();

		closeFilter->isCalculating = true;

		if ((!scriptCheck->isChecked() || !scriptCheck->isEnabled())) {
			_controller->autoSchedule(countSpin->value(), priorityCheck->isChecked(), multiCheck->isChecked());
		}
		else {
			if (Settings::instance()->getEnableConsole()) {
				if (RConsole::instance()->isHidden()) {
					RConsole::instance()->show();
				}
                RConsole::instance()->appendLog("开始执行Python脚本排班");
			}

			QString scriptPath = QDir(QCoreApplication::applicationDirPath()).filePath("scripts/" + _script);
			_controller->PythonSchedule(scriptPath, countSpin->value());
		}

		});

	connect(_controller, &EditorDutyController::scheduleFinish, &dialog, [&dialog, closeFilter]() {
		closeFilter->isCalculating = false;
		dialog.accept();
		});

	QScreen* screen = QGuiApplication::primaryScreen();
	QRect rect = screen->availableGeometry();
	int w = qBound(320, static_cast<int>(rect.width() * 0.15), 500);
	int h = qBound(280, static_cast<int>(rect.height() * 0.25), 500);
	dialog.setCentralLayout(mainLayout, w, h);
	dialog.exec();
}

#ifdef ENABLE_AI_SUPPORT
void EditPage::popAIDialog() {
	DialogAction dialog(this);
	dialog.setWindowTitle("AI 智能排班");
	QScreen* screen = QGuiApplication::primaryScreen();
	QRect screenRect = screen->availableGeometry();
	int w = qBound(380, static_cast<int>(screenRect.width() * 0.3), 600);
	int h = qBound(320, static_cast<int>(screenRect.height() * 0.5), 700);
	dialog.resize(w, h);
	dialog.setMinimumSize(380, 320);

	QStackedWidget* stack = new QStackedWidget(&dialog);
	QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
	mainLayout->addWidget(stack);

	QWidget* pageInput = new QWidget();
	QVBoxLayout* inputLayout = new QVBoxLayout(pageInput);

	QHBoxLayout* startLayout = new QHBoxLayout();
	startLayout->addStretch();
	startLayout->addWidget(new ElaText("开始日期", 13));
	startLayout->addSpacing(30);
	ElaCalendarPicker* startPicker = new ElaCalendarPicker(&dialog);
	startPicker->setFixedHeight(35);
	startPicker->setSelectedDate(QDate::currentDate());
	startLayout->addWidget(startPicker);
	startLayout->addStretch();
	inputLayout->addLayout(startLayout);

	QHBoxLayout* endLayout = new QHBoxLayout();
	endLayout->addStretch();
	endLayout->addWidget(new ElaText("结束日期", 13));
	endLayout->addSpacing(30);
	ElaCalendarPicker* endPicker = new ElaCalendarPicker(&dialog);
	endPicker->setFixedHeight(35);
	endPicker->setSelectedDate(QDate::currentDate().addDays(30));
	endLayout->addWidget(endPicker);
	endLayout->addStretch();
	inputLayout->addLayout(endLayout);

	ElaCheckBox* thinkCheck = new ElaCheckBox("启用深度思考");
	thinkCheck->setChecked(false);//默认快速模式
	inputLayout->addWidget(thinkCheck);

	ElaPushButton* propBtn = new ElaPushButton("选择属性");
	propBtn->setFixedSize(120, 36);

	ElaPlainTextEdit* promptEdit = new ElaPlainTextEdit(&dialog);
	promptEdit->setPlaceholderText("请输入特殊要求（如：周五张三不排班，每人只排一次）...");
	promptEdit->setMinimumHeight(150);
	inputLayout->addWidget(promptEdit);

	QHBoxLayout* btnLayout0 = new QHBoxLayout();
	btnLayout0->addStretch();
	ElaPushButton* okBtn = new ElaPushButton("开始计算");
	okBtn->setFixedSize(120, 36);
	ElaPushButton* cancelBtn0 = new ElaPushButton("取消");
	cancelBtn0->setFixedSize(120, 36);
	btnLayout0->addWidget(okBtn);
	btnLayout0->addSpacing(15);
	btnLayout0->addWidget(propBtn);
	btnLayout0->addSpacing(15);
	btnLayout0->addWidget(cancelBtn0);
	btnLayout0->addStretch();
	inputLayout->addLayout(btnLayout0);

	stack->addWidget(pageInput);

	QWidget* pageThinking = new QWidget();
	QVBoxLayout* thinkLayout = new QVBoxLayout(pageThinking);

	ElaProgressRing* progressRing = new ElaProgressRing(&dialog);
	progressRing->setIsBusying(false);
	progressRing->setFixedSize(50, 50);
	QHBoxLayout* progressLayout = new QHBoxLayout();
	progressLayout->addStretch();
	progressLayout->addWidget(progressRing);
	progressLayout->addStretch();
	thinkLayout->addLayout(progressLayout);

	ElaText* statusText = new ElaText("AI 正在思考...", 14);
	statusText->setAlignment(Qt::AlignCenter);
	thinkLayout->addWidget(statusText);

	ElaPlainTextEdit* logEdit = new ElaPlainTextEdit(&dialog);
	logEdit->setReadOnly(true);
	logEdit->setPlaceholderText("思考过程将显示在这里...");
	logEdit->setFixedHeight(250);
	thinkLayout->addWidget(logEdit);

	ElaPushButton* cancelBtn1 = new ElaPushButton("取消计算");
	cancelBtn1->setFixedSize(120, 36);
	QHBoxLayout* btnLayout1 = new QHBoxLayout();
	btnLayout1->addStretch();
	btnLayout1->addWidget(cancelBtn1);
	btnLayout1->addStretch();
	thinkLayout->addLayout(btnLayout1);

	stack->addWidget(pageThinking);

	stack->setCurrentIndex(0);

	AIServiceInterface* ai = AIServiceInterface::instance();

	connect(cancelBtn0, &ElaPushButton::clicked, &dialog, &QDialog::reject);
	connect(cancelBtn1, &ElaPushButton::clicked, this, [&dialog, this]() {
		_controller->abortSchedule();
		dialog.reject();
		});

	connect(okBtn, &ElaPushButton::clicked, this, [=, &dialog]() {
		QDate start = startPicker->getSelectedDate();
		QDate end = endPicker->getSelectedDate();
		if (start > end) {
			ElaMessageBar::warning(ElaMessageBarType::BottomLeft, "警告", "日期范围无效", 2000, &dialog);
			return;
		}
		bool enableThinking = thinkCheck->isChecked();
		logEdit->clear();
		progressRing->setIsBusying(true);
		stack->setCurrentIndex(1);

		_controller->requestAISchedule(promptEdit->toPlainText(), start, end, enableThinking, _selectedAIProperties);
		});

	connect(ai, &AIServiceInterface::aiProgress, &dialog,
		[logEdit](const QString& chunk) {
			QScrollBar* vbar = logEdit->verticalScrollBar();
			bool atBottom = (vbar->value() >= vbar->maximum() - 10);
			int oldPos = vbar->value();
			QTextCursor cursor(logEdit->document());
			cursor.movePosition(QTextCursor::End);
			cursor.insertText(chunk);
			if (atBottom) {
				vbar->setValue(vbar->maximum());
			}
			else {
				vbar->setValue(oldPos);
			}
			QApplication::processEvents();
		}, Qt::QueuedConnection);

	connect(ai, &AIServiceInterface::aiFinished, &dialog,
		[statusText]() {
			statusText->setText("生成完成，正在填入排班...");
		}, Qt::QueuedConnection);

	connect(_controller, &EditorDutyController::scheduleFinish, &dialog,
		[&dialog]() { dialog.accept(); });

	connect(&dialog, &QDialog::finished, &dialog, [ai, &dialog]() {
		disconnect(ai, &AIServiceInterface::aiProgress, &dialog, nullptr);
		disconnect(ai, &AIServiceInterface::aiFinished, &dialog, nullptr);
		});

	connect(propBtn, &ElaPushButton::clicked, this, &EditPage::showPropertySelectionDialog);

	dialog.exec();
}
#endif

#ifdef ENABLE_AI_SUPPORT
void EditPage::showPropertySelectionDialog()
{
	auto* mComp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
	MemberInfoController* mController = mComp->getController();
	QStringList allProps = mController->getAllPropertyNames();

	DialogAction dlg(this);
	dlg.setWindowTitle("选择提交给AI的属性");
	QSize screenSize = QGuiApplication::primaryScreen()->availableSize();
	dlg.resize(screenSize.width() * 0.25, screenSize.height() * 0.4);
	dlg.setMinimumSize(280, 220);

	QVBoxLayout* mainLayout = new QVBoxLayout(&dlg);

	ElaScrollArea* scrollArea = new ElaScrollArea(&dlg);
	scrollArea->setWidgetResizable(true);
	QWidget* scrollWidget = new QWidget();
	QVBoxLayout* vLayout = new QVBoxLayout(scrollWidget);
	vLayout->setContentsMargins(0, 0, 0, 0);

	QWidget* gridWidget = new QWidget();
	QGridLayout* gridLayout = new QGridLayout(gridWidget);
	gridLayout->setSpacing(5);

	QSet<QString> localProps = _selectedAIProperties;
	QList<ElaCheckBox*> checkBoxes;
	bool hasProps = false;
	for (const QString& prop : allProps) {
		if (prop == "姓名") continue;
		ElaCheckBox* cb = new ElaCheckBox(prop, gridWidget);
		cb->setChecked(localProps.contains(prop));
		int row = checkBoxes.size() / 4;
		int col = checkBoxes.size() % 4;
		gridLayout->addWidget(cb, row, col);
		checkBoxes << cb;
		hasProps = true;
	}
	if (!hasProps) {
		ElaText* noPropLabel = new ElaText("暂无额外属性", gridWidget);
		noPropLabel->setTextPixelSize(20);
		noPropLabel->setAlignment(Qt::AlignCenter);
		gridLayout->addWidget(noPropLabel, 0, 0, 1, 4);
	}

	vLayout->addWidget(gridWidget);
	vLayout->addStretch();
	scrollArea->setWidget(scrollWidget);
	mainLayout->addWidget(scrollArea, 1);

	QHBoxLayout* btnLayout = new QHBoxLayout();
	btnLayout->addStretch();
	ElaPushButton* okBtn = new ElaPushButton("确定");
	okBtn->setFixedSize(80, 36);
	ElaPushButton* cancelBtn = new ElaPushButton("取消");
	cancelBtn->setFixedSize(80, 36);
	ElaPushButton* toggleBtn = new ElaPushButton("全选", &dlg);
	toggleBtn->setFixedSize(80, 36);
	btnLayout->addWidget(okBtn);
	btnLayout->addSpacing(10);
	btnLayout->addWidget(cancelBtn);
	btnLayout->addSpacing(10);
	btnLayout->addWidget(toggleBtn);
	btnLayout->addStretch();
	mainLayout->addLayout(btnLayout);

	auto updateToggleBtnText = [toggleBtn, checkBoxes]() {
		if (checkBoxes.isEmpty()) {
			toggleBtn->setText("全选");
			toggleBtn->setEnabled(false);
			return;
		}
		toggleBtn->setEnabled(true);
		bool allChecked = true;
		for (auto* cb : checkBoxes) {
			if (!cb->isChecked()) { allChecked = false; break; }
		}
		toggleBtn->setText(allChecked ? "取消全选" : "全选");
		};
	updateToggleBtnText();

	connect(toggleBtn, &ElaPushButton::clicked, [=]() mutable {
		bool allChecked = (toggleBtn->text() == "全选");
		for (auto* cb : checkBoxes) {
			cb->setChecked(allChecked);
		}
		updateToggleBtnText();
		});

	connect(cancelBtn, &ElaPushButton::clicked, &dlg, &QDialog::reject);
	connect(okBtn, &ElaPushButton::clicked, &dlg, &QDialog::accept);

	if (dlg.exec() == QDialog::Accepted) {
		localProps.clear();
		for (auto* cb : checkBoxes) {
			if (cb->isChecked()) {
				localProps.insert(cb->text());
			}
		}
		_selectedAIProperties = localProps;
	}
}
#endif

void EditPage::changeScriptMode(bool enable, QString script) {
	_isScriptMode = enable;
	_script = script;
	qDebug() << "切换脚本模式，状态：" << enable << " 脚本内容：" << script;
}

void EditPage::changeScript(QString script) {
	_script = script;
	qDebug() << " 脚本内容：" << script;
}

void EditPage::hideEvent(QHideEvent* event) {
	//_selectDock && !_selectDock->isFloating()
	if (_selectDock) {
		_selectDock->hide();
	}
	ElaScrollPage::hideEvent(event);
}

bool AIDialogCloseFilter::eventFilter(QObject* watched, QEvent* event) {
	if (event->type() == QEvent::Close ||
		(event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)) {

		if (isCalculating) {
			auto resBtn = QMessageBox::question(static_cast<QWidget*>(watched),
				"提示", "AI 正在计算排班，确定要中止并退出吗？",
				QMessageBox::Yes | QMessageBox::No);

			if (resBtn == QMessageBox::Yes) {
				if (onAbort) onAbort();
				return false;
			}
			else {
				event->ignore();
				return true;
			}
		}
	}
	return QObject::eventFilter(watched, event);
}

#ifdef ENABLE_AI_SUPPORT
void EditPage::changeAIMode(bool isAIMode) {
	_AIBtn->setVisible(isAIMode);
	_isAIMode = isAIMode;
}

void EditPage::checkAISchedule(bool enable) {
	changeAIMode(enable);
}
#endif