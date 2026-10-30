#include "memberinformationdock.h"
#include "ElaLineEdit.h"
#include "ElaScrollArea.h"
#include <QLabel>
#include <QVBoxLayout>
#include "ElaTheme.h"
#include <QStyle>
#include "ElaText.h"
#include <QFormLayout>
#include "memberpage.h"
#include "ElaPlainTextEdit.h"
#include "ElaPushButton.h"
#include <QDate>
#include "windialog.h"
#include "RTypies.h"
#include "componentmanager.h"
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include "ElaCalendarPicker.h"
#include "ElaMessageBar.h"
#include "projectmanager.h"
#include "dialogaction.h"
#include <QProgressBar>
#include "ElaCheckBox.h"
#include <QFontMetrics>

MemberInformationDock::MemberInformationDock(QWidget* parent)
	: ElaDockWidget(parent)
{
    auto* sComp = ComponentManager::instance()->getComponent<ScheduleComponent>();
    _sController = sComp->getController();
    auto* mComp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    _controller = mComp->getController();

    setWindowTitle("成员信息");
	setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    connect(_controller, &MemberInfoController::requestUpdateSchedule, _sController, &ScheduleController::onMemberDataSelected);

    connect(_controller, &MemberInfoController::propertyListChanged, this, [=]() {
        if (this->isVisible() && _currentRow >= 0) {
            QVariantMap details = _controller->getMemberDetails(_currentRow);
            setMemberDetails(details, _currentRow);
        }
        });

    // QList insert/remove can relocate every MemberData, including the one displayed here.
    auto detachSchedule = [this]() { Q_EMIT _controller->requestUpdateSchedule(nullptr); };
    connect(mComp->getModel(), &QAbstractItemModel::rowsAboutToBeInserted, this, detachSchedule);
    connect(mComp->getModel(), &QAbstractItemModel::rowsAboutToBeRemoved, this, detachSchedule);
    connect(mComp->getModel(), &QAbstractItemModel::modelAboutToBeReset, this, [this, detachSchedule]() {
        if (_currentRow >= 0 && _noteEdit) _controller->updateMemberNote(_currentRow, _noteEdit->toPlainText());
        detachSchedule();
        _currentRow = -1;
        _currentMemberId = {};
        hide();
    });
    auto rebindSchedule = [this, mComp]() {
        if (_currentMemberId.isNull()) return;
        _currentRow = mComp->getModel()->getRowById(_currentMemberId);
        if (_currentRow < 0) {
            _currentMemberId = {};
            hide();
            return;
        }
        Q_EMIT _controller->requestUpdateSchedule(_controller->getMemberDataPtr(_currentRow));
    };
    connect(mComp->getModel(), &QAbstractItemModel::rowsInserted, this, rebindSchedule);
    connect(mComp->getModel(), &QAbstractItemModel::rowsRemoved, this, rebindSchedule);

    connect(mComp->getModel(), &QAbstractItemModel::dataChanged, this, [=](const QModelIndex& topLeft, const QModelIndex& bottomRight, const QList<int>& roles) {
        if (this->isVisible() && _currentRow >= topLeft.row() && _currentRow <= bottomRight.row()) {
            QVariantMap details = _controller->getMemberDetails(_currentRow);
            for (auto it = details.constBegin(); it != details.constEnd(); ++it) {
                ElaLineEdit* edit = this->findChild<ElaLineEdit*>(it.key());
                if (edit && edit->text() != it.value().toString()) {
                    edit->setText(it.value().toString());
                }
            }
            QString filename = details.value("_scheduleFileSource").toString();
            if (_scheduleFileLabel) {
                if (!filename.isEmpty()) {
                    QFontMetrics fm(_scheduleFileLabel->font());
                    int availableWidth = this->width() - 150;
                    _scheduleFileLabel->setText(fm.elidedText(filename, Qt::ElideRight, qMax(50, availableWidth)));
                    _scheduleFileLabel->setToolTip(filename);
                }
                else {
                    _scheduleFileLabel->setText("");
                }
            }
        }
        });
    
    initUi();
}

MemberInformationDock::~MemberInformationDock()
{}

void MemberInformationDock::initUi()
{
    this->setMinimumWidth(350);
    ElaScrollArea* scrollArea = new ElaScrollArea(this);
    scrollArea->setWidgetResizable(true);

    QWidget* container = new QWidget();
    container->setObjectName("dockContainer");
    container->setStyleSheet("#dockContainer { background: transparent; }");

    _mainLayout = new QVBoxLayout(container);
    _formLayout = new QFormLayout();

    _formLayout->setSpacing(15);
    _mainLayout->addLayout(_formLayout);
    _mainLayout->addStretch();

    _noteEdit = new ElaPlainTextEdit(this);
    _noteEdit->setFixedHeight(120);
    _noteEdit->installEventFilter(this);
    _mainLayout->addWidget(new ElaText("备注：", 14));
    _mainLayout->addWidget(_noteEdit);

    _mainLayout->addSpacing(10);

    QHBoxLayout* scheduleTitleLayout = new QHBoxLayout();
    scheduleTitleLayout->addWidget(new ElaText("个人时间表：", 14));
    _scheduleFileLabel = new ElaText("", 12);
    _scheduleFileLabel->setStyleSheet("color: gray;");
    scheduleTitleLayout->addWidget(_scheduleFileLabel, 1);
    _mainLayout->addLayout(scheduleTitleLayout);

    _sController->setupView(_mainLayout);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    _addRowBtn = new ElaPushButton("添加时间段");
    _addColBtn = new ElaPushButton("添加日期");
    _importBtn = new ElaPushButton("导入时间表");
    _exportBtn = new ElaPushButton("导出时间表");
    _importBtn->resize(180, 60);
    _exportBtn->resize(180, 60);
    btnLayout->addStretch();
    btnLayout->addWidget(_addRowBtn);
    btnLayout->addWidget(_addColBtn);
    btnLayout->addWidget(_importBtn);
    btnLayout->addWidget(_exportBtn);
    _mainLayout->addLayout(btnLayout);

    _mainLayout->addStretch();
    scrollArea->setWidget(container);
    this->setWidget(scrollArea);

    connect(ElaTheme::getInstance(), &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode themeMode) {
        this->style()->unpolish(this);
        this->style()->polish(this);
        });

    connect(_addColBtn, &ElaPushButton::clicked, [=]() {
        _sController->addNextDay();
        });

    connect(_addRowBtn, &ElaPushButton::clicked, [=]() {
        Q_EMIT popWinDialog(RTypies::WinDialog::AddMemberTimeRow);
        });

    connect(_importBtn, &ElaPushButton::clicked, this, &MemberInformationDock::importSchedule);
    connect(_exportBtn, &ElaPushButton::clicked, this, &MemberInformationDock::exportSchedule);

    connect(ProjectManager::instance(), &ProjectManager::scheduleExportFinished, this, [=](bool ok) {
        QWidget* parent = this->window() ? this->window() : this;
        if (ok) {
            if (this->isFloating()) {
                ElaMessageBar::success(ElaMessageBarType::BottomLeft, "成功", "数据导出成功", 2000, parent);
            }
            else {
                Q_EMIT finishExportSchedule(ok);
            }
        }
        else {
            if (this->isFloating()) {
                ElaMessageBar::error(ElaMessageBarType::BottomLeft, "失败", "数据导出失败", 2000, parent);
            }
            else {
                Q_EMIT finishExportSchedule(ok);
            }
        }
        });
}

void MemberInformationDock::setMemberDetails(const QVariantMap& details, const int row)
{
    auto* model = ComponentManager::instance()->getComponent<MemberInfoComponent>()->getModel();
    auto* member = model->getMemberDataPtr(row);
    if (!member) return;
    const QUuid editingMemberId = member->id;
    if (_currentRow >= 0 && _noteEdit && _currentMemberId != editingMemberId) {
        _controller->updateMemberNote(_currentRow, _noteEdit->toPlainText());
    }
    _currentRow = row;
    _currentMemberId = editingMemberId;

    //只清理 FormLayout 里的动态属性框
    clearLayout();
    //重新生成属性输入框
    QList<QString> allProps = _controller->getAllPropertyNames();
    for (const QString& key : allProps) {
        if (!details.contains(key)) continue;
        ElaLineEdit* editLine = new ElaLineEdit(this);
        editLine->setText(details.value(key).toString());
        editLine->setObjectName(key);
        connect(editLine, &ElaLineEdit::editingFinished, this, [this, editLine, editingMemberId]() {
            QString attrName = editLine->objectName();
            QString newValue = editLine->text();
            _controller->updateMemberProperty(editingMemberId, attrName, newValue);
            });
        _formLayout->addRow(new ElaText(key + ":", 14), editLine);
    }

    _controller->refreshMemberDutyCount(row);
    int currentDutyCount = _controller->getDutyCount(row);
    QWidget* countRowWidget = new QWidget(this);
    QHBoxLayout* countHLayout = new QHBoxLayout(countRowWidget);
    countHLayout->setContentsMargins(0, 0, 0, 0);

    ElaLineEdit* countEdit = new ElaLineEdit(this);
    countEdit->setText(QString::number(currentDutyCount));
    countEdit->setReadOnly(true);
    countEdit->setFocusPolicy(Qt::NoFocus);
    countEdit->setStyleSheet("background-color: transparent; border: none;");

    ElaCheckBox* enableCheckBox = new ElaCheckBox("是否参与", this);
    enableCheckBox->setChecked(details.value("_enableScheduling", true).toBool());
    connect(enableCheckBox, &ElaCheckBox::toggled, this, [this, editingMemberId, model](bool checked) {
        _controller->setEnableScheduling(model->getRowById(editingMemberId), checked);
        });

    countHLayout->addWidget(countEdit);
    countHLayout->addStretch();
    countHLayout->addWidget(enableCheckBox);

    _formLayout->addRow(new ElaText("已排班次数:", 14), countRowWidget);

    QString filename = details.value("_scheduleFileSource").toString();
    if (!filename.isEmpty()) {
        QFontMetrics fm(_scheduleFileLabel->font());
        int availableWidth = this->width() - 150;
        _scheduleFileLabel->setText(fm.elidedText(filename, Qt::ElideRight, qMax(50, availableWidth)));
        _scheduleFileLabel->setToolTip(filename);
    }
    else {
        _scheduleFileLabel->setText("");
        _scheduleFileLabel->setToolTip("");
    }

    _noteEdit->setPlainText(_controller->getMemberNote(_currentRow));
    
    void* ptr = _controller->getMemberDataPtr(row);
    if (ptr) {
        Q_EMIT _controller->requestUpdateSchedule(ptr);
    }
    
    _sController->updateViewHeight();
    this->show();
}

void MemberInformationDock::importSchedule() {
    auto* memberComponent = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    if (!memberComponent || _currentRow < 0 || _currentRow >= memberComponent->getModel()->rowCount()) return;
    const QPersistentModelIndex importTarget(memberComponent->getModel()->index(_currentRow, 0));
    const QUuid importMemberId = memberComponent->getModel()->getMemberDataPtr(_currentRow)->id;
    QString filePath = QFileDialog::getOpenFileName(this, "导入课表", "", "RabbitRoll Schedule (*.rsch);");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "导入失败", "无法读取课表文件。");
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    QJsonObject root = document.object();
    file.close();

    QString dataType = root["dataType"].toString();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || root["app"].toString() != "rabbitroll"
        || (dataType != "course_schedule" && dataType != "schedule")) {
        QMessageBox::warning(this, "导入失败", "该文件不是有效的 RabbitRoll 时间表。");
        return;
    }
    if (dataType == "course_schedule" && root["school"].toString() != "jwxt.wit.edu.cn") {
        QMessageBox::warning(this, "导入失败", "当前仅支持导入 jwxt.wit.edu.cn 的课程表。");
        return;
    }
    QDate startDate = QDate::currentDate();
    int overwritePolicy = -1;
    if (dataType == "course_schedule") {
        DialogAction dateDialog(this);
        dateDialog.setWindowTitle("选择学期起点");
        QVBoxLayout* contentLayout = new QVBoxLayout();
        ElaText* hintText = new ElaText("请指定该课程表第一周的第一天（周一）：", 14);
        contentLayout->addWidget(hintText);
        ElaCalendarPicker* calPicker = new ElaCalendarPicker();
        if (_lastChosenStartDate.isValid()) {
            calPicker->setSelectedDate(_lastChosenStartDate);
        }
        contentLayout->addWidget(calPicker);

        QHBoxLayout* btnLay = new QHBoxLayout();
        btnLay->addStretch();
        ElaPushButton* okBtn = new ElaPushButton("开始导入");
        ElaPushButton* cancelBtn = new ElaPushButton("取消");
        btnLay->addWidget(okBtn);
        btnLay->addWidget(cancelBtn);
        contentLayout->addLayout(btnLay);
        dateDialog.setCentralLayout(contentLayout, 320, 150);

        connect(okBtn, &ElaPushButton::clicked, &dateDialog, &QDialog::accept);
        connect(cancelBtn, &ElaPushButton::clicked, &dateDialog, &QDialog::reject);

        if (dateDialog.exec() != QDialog::Accepted) return;
        startDate = calPicker->getSelectedDate();
        if (!startDate.isValid() || startDate.dayOfWeek() != 1) {
            QMessageBox::warning(this, "导入失败", "学期起点必须选择第一周的周一。");
            return;
        }
        _lastChosenStartDate = startDate;
    }
    else if (dataType == "schedule") {
        auto res = QMessageBox::question(this, "导入确认",
            "检测可能导入的日期与当前数据冲突，是否覆盖?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (res == QMessageBox::Cancel) return;
        overwritePolicy = (res == QMessageBox::Yes) ? 1 : 0;
    }

    QProgressDialog* progress = new QProgressDialog("正在处理数据，请稍候...", QString(), 0, 0, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setCancelButton(nullptr);
    progress->setRange(0, 0);
    progress->show();
    QProgressBar* bar = progress->findChild<QProgressBar*>();
    if (bar) bar->setAlignment(Qt::AlignCenter);

    _importBtn->setEnabled(false);

    auto* pm = ProjectManager::instance();

    connect(pm, &ProjectManager::scheduleImportFinished, this, [=](bool ok, const QJsonObject& res) {
        progress->close();
        progress->deleteLater();
        _importBtn->setEnabled(true);

        QWidget* parent = this->window() ? this->window() : this;
        const auto* targetData = importTarget.isValid()
            ? memberComponent->getModel()->getMemberDataPtr(importTarget.row()) : nullptr;
        const bool targetExists = targetData && targetData->id == importMemberId;
        bool imported = false;
        if (ok && targetExists && !res.isEmpty()) {
            _controller->selectAndShowMember(importMemberId);
            imported = _sController->handleImportedSchedule(res, this, overwritePolicy);
        }
        if (imported) {
            QFileInfo fileInfo(filePath);
            _controller->setScheduleFileSource(importTarget.row(), fileInfo.fileName());
            Q_EMIT propertyChanged("schedule", "imported");
            if (this->isFloating()) {
                ElaMessageBar::success(ElaMessageBarType::BottomLeft, "成功", "数据导入成功", 2000, parent);
            }
            else {
                Q_EMIT finishImportSchedule(ok);
            }
        }
        else {
            // A cancelled merge is deliberately left untouched.
            if (ok && targetExists && !res.isEmpty()) return;
            if (this->isFloating()) {
                ElaMessageBar::error(ElaMessageBarType::BottomLeft, "失败", targetExists ? "课表未包含可导入的数据" : "原成员已被删除或项目已切换，请重新导入", 2000, parent);
            }
            else {
                Q_EMIT finishImportSchedule(false);
            }
        }
        }, Qt::SingleShotConnection);

    pm->importScheduleFromFile(filePath, startDate);
}

bool MemberInformationDock::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _noteEdit) {
        if (event->type() == QEvent::FocusOut) {
            QString newNote = _noteEdit->toPlainText();
            Q_EMIT noteChanged(_currentRow, newNote);
        }
    }
    return ElaDockWidget::eventFilter(watched, event);
}

void MemberInformationDock::clearLayout()
{
    if (!_formLayout) return;

    while (_formLayout->count() > 0) {
        QLayoutItem* item = _formLayout->takeAt(0);
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}
void MemberInformationDock::showEvent(QShowEvent* event)
{
	_controller->changeViewClickMode(RTypies::View::SingleClike);
    ElaDockWidget::showEvent(event);
}

void MemberInformationDock::hideEvent(QHideEvent* event)
{
    if (_currentRow >= 0 && _noteEdit) _controller->updateMemberNote(_currentRow, _noteEdit->toPlainText());
    _currentRow = -1;
    _currentMemberId = {};
    Q_EMIT _controller->requestUpdateSchedule(nullptr);
    _controller->changeViewClickMode(RTypies::View::DoubleClick);
    ElaDockWidget::hideEvent(event);
}

void MemberInformationDock::resizeEvent(QResizeEvent* event) {
    ElaDockWidget::resizeEvent(event);
    if (_scheduleFileLabel && _currentRow >= 0) {
        QVariantMap details = _controller->getMemberDetails(_currentRow);
        QString filename = details.value("_scheduleFileSource").toString();
        if (!filename.isEmpty()) {
            QFontMetrics fm(_scheduleFileLabel->font());
            int availableWidth = this->width() - 150;
            _scheduleFileLabel->setText(fm.elidedText(filename, Qt::ElideRight, qMax(50, availableWidth)));
        }
    }
}

