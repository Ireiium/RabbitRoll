#include "batchrschimportdialog.h"
#include "memberinfocontroller.h"
#include "settings.h"
#include "ElaCalendarPicker.h"
#include "ElaComboBox.h"
#include "ElaCheckBox.h"
#include "ElaPushButton.h"
#include "ElaScrollPageArea.h"
#include "ElaTableView.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include <QDate>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QLineEdit>
#include <QScreen>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <algorithm>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
class ImportConfirmationDialog final : public DialogAction {
public:
    using DialogAction::DialogAction;
#ifdef Q_OS_WIN
protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override {
        // Keep Qt's per-window DPI sizing instead of Ela's application-wide DPI minimum.
        if (eventType == "windows_generic_MSG" && message
            && static_cast<const MSG*>(message)->message == WM_GETMINMAXINFO)
            return QDialog::nativeEvent(eventType, message, result);
        return DialogAction::nativeEvent(eventType, message, result);
    }
#endif
};

class PreviewDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setClipRect(option.rect);
        QStyledItemDelegate::paint(painter, option, index);
        painter->restore();
    }
protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        option->text = option->fontMetrics.elidedText(option->text, Qt::ElideRight, qMax(0, option->rect.width() - 12));
    }
};

QString memberLabel(const MemberInfoModel::MemberData& member, int index) {
    const auto name = member.properties.value("姓名").toString();
    const auto department = member.properties.value("部门").toString();
    return QString("%1%2  · #%3").arg(name, department.isEmpty() ? QString{} : " · " + department).arg(index + 1);
}
}

BatchRschImportDialog::BatchRschImportDialog(MemberInfoController* controller, QWidget* parent)
    : DialogAction(parent), _controller(controller) {
    setObjectName("batchRschImportDialog");
    setWindowTitle("通过 RSCH 批量导入");
    setAcceptDrops(true);
    auto* layout = new QVBoxLayout;
    layout->setSpacing(12);
    layout->addWidget(new ElaText("批量导入成员与个人时间表", 22, this));
    auto* area = new ElaScrollPageArea(this);
    auto* form = new QFormLayout(area);
    form->setContentsMargins(16, 12, 16, 12);
    form->setVerticalSpacing(8);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    _format = new ElaComboBox(this);
    _format->setObjectName("rschFileNameFormat");
    _format->setEditable(true);
    _format->addItems({"姓名", "部门 姓名"});
    _format->setCurrentText(Settings::instance()->getRschImportFormat());
    _format->setToolTip("自动解析属性或新建成员时使用；手动对应已有成员不受文件名格式限制。");
    form->addRow(new ElaText("文件名格式：", 15, this), _format);
    _example = new ElaText("", 14, this);
    _example->setTextFormat(Qt::PlainText);
    _example->setWordWrap(true);
    form->addRow({}, _example);
    _mode = new ElaComboBox(this);
    _mode->setObjectName("rschImportMode");
    _mode->addItems({"匹配已有成员，缺失时新建", "仅更新已有成员"});
    form->addRow(new ElaText("导入方式：", 15, this), _mode);
    auto* dateRow = new QHBoxLayout;
    _semester = new ElaCalendarPicker(this);
    _semester->setObjectName("rschSemesterStart");
    _semesterStart = Settings::instance()->getRschSemesterStart();
    _semester->setSelectedDate(_semesterStart);
    dateRow->addWidget(_semester);
    _semesterHint = new ElaText("", 14, this);
    _semesterHint->setWordWrap(true);
    dateRow->addWidget(_semesterHint, 1);
    form->addRow(new ElaText("学期起点：", 15, this), dateRow);
    auto* policies = new QHBoxLayout;
    _merge = new ElaComboBox(this);
    _merge->setObjectName("rschSchedulePolicy");
    _merge->addItems({"合并：保留原有不可值班时间", "替换：清空该成员原时间表"});
    _merge->setToolTip("合并保留原有不可值班时间；替换会清空该成员的整个原时间表，再导入本文件。");
    _attributes = new ElaComboBox(this);
    _attributes->setObjectName("rschAttributePolicy");
    _attributes->addItems({"属性：仅补充空值", "属性：覆盖文件名中的属性"});
    _attributes->setToolTip("已有成员的姓名始终保留；此选项仅控制其他属性。");
    policies->addWidget(_merge, 1);
    policies->addWidget(_attributes, 1);
    form->addRow(new ElaText("已有数据：", 15, this), policies);
    area->setFixedHeight(form->sizeHint().height() + 24);
    layout->addWidget(area);
    auto* files = new QHBoxLayout;
    _chooseFiles = new ElaPushButton("选择 RSCH 文件", this);
    _chooseFiles->setObjectName("rschChooseFiles");
    _chooseFolder = new ElaPushButton("选择文件夹", this);
    _refresh = new ElaPushButton("重新读取", this);
    auto* remove = new ElaPushButton("移除选中行", this);
    files->addWidget(_chooseFiles);
    files->addWidget(_chooseFolder);
    files->addWidget(_refresh);
    files->addWidget(remove);
    files->addStretch();
    _toggleSelection = new ElaPushButton("全选", this);
    _toggleSelection->setObjectName("rschToggleSelection");
    _toggleSelection->setEnabled(false);
    files->addWidget(_toggleSelection);
    layout->addLayout(files);
    _summary = new ElaText("", 14, this);
    _summary->setObjectName("rschImportSummary");
    _summary->setTextFormat(Qt::PlainText);
    _summary->setWordWrap(true);
    _summary->hide();
    layout->addWidget(_summary);
    _table = new ElaTableView(this);
    _table->setObjectName("rschImportPreview");
    _table->setAcceptDrops(false);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table->verticalHeader()->hide();
    _table->setAlternatingRowColors(true);
    _model = new QStandardItemModel(this);
    _model->setHorizontalHeaderLabels({"导入", "RSCH 文件", "对应成员", "文件名属性", "导入日期范围", "处理结果"});
    _table->setModel(_model);
    _table->setItemDelegate(new PreviewDelegate(_table));
    _table->setWordWrap(false);
    _table->setTextElideMode(Qt::ElideRight);
    _table->horizontalHeader()->setStretchLastSection(true);
    const int widths[] = {46, 180, 220, 180, 180, 230};
    for (int i = 0; i < 6; ++i) _table->setColumnWidth(i, widths[i]);
    layout->addWidget(_table, 1);
    auto* hint = new ElaText("只导入勾选项。文件名用于辅助匹配，无法识别时可在“对应成员”中手动选择已有成员。", 14, this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    _import = new ElaPushButton("确认导入", this);
    _import->setObjectName("rschImportConfirm");
    _import->setEnabled(false);
    _import->setLightDefaultColor(QColor("#0078d4"));
    _import->setLightHoverColor(QColor("#006cc1"));
    _import->setDarkDefaultColor(QColor("#0078d4"));
    _import->setDarkHoverColor(QColor("#1686d9"));
    _import->setLightTextColor(Qt::white);
    _import->setDarkTextColor(Qt::white);
    auto* cancel = new ElaPushButton("取消", this);
    buttons->addWidget(_import);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    setCentralLayout(layout, 1100, 760);
    setIsFixedSize(false);
    setMinimumSize(860, 620);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    if (const auto* screen = QGuiApplication::primaryScreen())
        resize(qMin(1100, screen->availableGeometry().width() - 40), qMin(760, screen->availableGeometry().height() - 40));
    _previewTimer = new QTimer(this);
    _previewTimer->setSingleShot(true);
    _previewTimer->setInterval(180);
    connect(_previewTimer, &QTimer::timeout, this, &BatchRschImportDialog::rebuildPreview);
    connect(_format, &QComboBox::editTextChanged, this, &BatchRschImportDialog::schedulePreview);
    for (auto* combo : {_mode, _merge, _attributes})
        connect(combo, &QComboBox::currentIndexChanged, this, &BatchRschImportDialog::schedulePreview);
    connect(_semester, &ElaCalendarPicker::selectedDateChanged, this, [this](QDate date) {
        _semesterStart = date;
        schedulePreview();
    });
    connect(_controller, &MemberInfoController::membersChanged, this, [this]() { if (!_applying) schedulePreview(); });
    connect(_chooseFiles, &QPushButton::clicked, this, [this]() {
        addFiles(QFileDialog::getOpenFileNames(this, "选择时间表文件（可多选）", {}, "RabbitRoll 时间表 (*.rsch)"));
    });
    connect(_chooseFolder, &QPushButton::clicked, this, [this]() {
        const auto path = QFileDialog::getExistingDirectory(this, "选择时间表所在文件夹");
        if (!path.isEmpty()) addFiles({path});
    });
    connect(_refresh, &QPushButton::clicked, this, &BatchRschImportDialog::readFiles);
    connect(_toggleSelection, &QPushButton::clicked, this, &BatchRschImportDialog::toggleSelection);
    connect(remove, &QPushButton::clicked, this, [this]() {
        if (_reading) return;
        const auto indexes = _table->selectionModel()->selectedRows();
        for (const auto& index : indexes) {
            const auto path = _rows.value(index.row()).path;
            _paths.removeAll(path);
            _targets.remove(path);
            _skipped.remove(path);
        }
        readFiles();
    });
    connect(_model, &QStandardItemModel::itemChanged, this, [this](QStandardItem* item) {
        if (_displaying || item->column() != 0) return;
        const auto path = _rows.value(item->row()).path;
        if (item->checkState() == Qt::Checked) _skipped.remove(path); else _skipped.insert(path);
        schedulePreview();
    });
    connect(_import, &QPushButton::clicked, this, &BatchRschImportDialog::importSelected);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(eTheme, &ElaTheme::themeModeChanged, this, &BatchRschImportDialog::updateThemeColors);
    updateThemeColors();
    schedulePreview();
}

RschImport::Options BatchRschImportDialog::options() const {
    RschImport::Options value;
    value.format = _format->currentText();
    value.semesterStart = _semesterStart;
    value.existingOnly = _mode->currentIndex() == 1;
    value.replaceSchedule = _merge->currentIndex() == 1;
    value.overwriteProperties = _attributes->currentIndex() == 1;
    value.targets = _targets;
    value.skipped = _skipped;
    return value;
}

void BatchRschImportDialog::addFiles(const QStringList& paths) {
    if (paths.isEmpty() || _reading) return;
    QStringList candidates;
    for (const auto& path : paths) {
        const QFileInfo file(path);
        if (file.isDir()) {
            for (const auto& entry : QDir(path).entryInfoList({"*.rsch"}, QDir::Files | QDir::Readable | QDir::NoSymLinks, QDir::Name))
                candidates.append(entry.absoluteFilePath());
        } else candidates.append(file.absoluteFilePath());
    }
    for (const auto& path : candidates)
        if (!_paths.contains(path, Qt::CaseInsensitive)) _paths.append(path);
    if (_paths.size() > 500) {
        _paths = _paths.first(500);
        _notice = "每批最多选择 500 个文件，请将其余文件分批导入。";
    }
    readFiles();
}

void BatchRschImportDialog::readFiles() {
    if (_reading) return;
    _reading = true;
    ++_generation;
    const auto readGeneration = ++_readGeneration;
    _previewTimer->stop();
    _import->setEnabled(false);
    _toggleSelection->setEnabled(false);
    _table->setEnabled(false);
    for (auto* button : {_chooseFiles, _chooseFolder, _refresh}) button->setEnabled(false);
    _summary->setText("正在读取并校验 RSCH 文件…");
    _summary->setVisible(!_paths.isEmpty());
    auto* watcher = new QFutureWatcher<QList<RschImport::Source>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, readGeneration]() {
        const auto files = watcher->result();
        watcher->deleteLater();
        if (readGeneration != _readGeneration) return;
        _sources = files;
        _reading = false;
        for (auto* button : {_chooseFiles, _chooseFolder, _refresh}) button->setEnabled(true);
        schedulePreview();
    });
    const auto paths = _paths;
    watcher->setFuture(QtConcurrent::run([paths]() { return RschImport::read(paths); }));
}

void BatchRschImportDialog::schedulePreview() {
    ++_generation;
    _import->setEnabled(false);
    _table->setEnabled(false);
    _toggleSelection->setEnabled(false);
    _semesterHint->setText(_semesterStart.isValid() && _semesterStart.dayOfWeek() == 1
        ? "第一周周一；普通时间表仍使用文件内日期"
        : "尚未设置：教务课表需选择第一周的周一；普通时间表无需设置");
    const auto format = _format->currentText().simplified();
    _example->setText(format == "部门 姓名" ? "例如：综合事务部 张三.rsch → 部门：综合事务部，姓名：张三"
        : format == "姓名" ? "例如：张三.rsch → 姓名：张三；也可直接输入“部门 年级 姓名”等格式"
        : "按格式顺序用空格分隔文件名；确认下方解析出的属性与成员对应关系");
    if (!_reading) {
        _summary->setText(_paths.isEmpty() ? QString{} : "正在生成预览…");
        _summary->setVisible(!_paths.isEmpty());
        _previewTimer->start();
    }
}

void BatchRschImportDialog::rebuildPreview() {
    const auto generation = _generation;
    _revision = _controller->importRevision();
    _members = _controller->importSnapshot();
    const auto members = _members;
    const auto files = _sources;
    const auto config = options();
    auto* watcher = new QFutureWatcher<QList<RschImport::Row>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation]() {
        const auto rows = watcher->result();
        watcher->deleteLater();
        if (generation != _generation || _reading) return;
        _rows = rows;
        displayPreview();
    });
    watcher->setFuture(QtConcurrent::run([files, members, config]() { return RschImport::preview(files, members, config); }));
}

void BatchRschImportDialog::displayPreview() {
    _displaying = true;
    _model->removeRows(0, _model->rowCount());
    int selected = 0, created = 0, problems = 0;
    bool hasUnselectedValidRow = false;
    bool blocked = false;
    for (int index = 0; index < _rows.size(); ++index) {
        const auto& row = _rows[index];
        QList<QStandardItem*> items;
        for (int i = 0; i < 6; ++i) items.append(new QStandardItem);
        items[0]->setCheckable(true);
        items[0]->setCheckState(row.selected ? Qt::Checked : Qt::Unchecked);
        items[1]->setText(QFileInfo(row.path).fileName());
        items[1]->setToolTip(row.path);
        QStringList properties;
        for (auto p = row.properties.cbegin(); p != row.properties.cend(); ++p)
            properties.append(p.key() + "：" + p.value().toString());
        items[3]->setText(properties.join("；"));
        items[3]->setToolTip(items[3]->text());
        items[4]->setText(row.dateRange);
        items[4]->setToolTip(row.dateRange);
        const auto state = !row.error.isEmpty() ? row.error : !row.selected ? "已跳过"
            : row.create ? "将新建成员并导入时间表" : row.warning.isEmpty() ? "将更新已有成员的时间表" : row.warning;
        items[5]->setText(state);
        items[5]->setToolTip(state);
        _model->appendRow(items);
        auto* checkArea = new QWidget(_table);
        auto* checkLayout = new QHBoxLayout(checkArea);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        auto* check = new ElaCheckBox(checkArea);
        check->setChecked(row.selected);
        check->setAccessibleName("导入 " + QFileInfo(row.path).fileName());
        checkLayout->addWidget(check, 0, Qt::AlignCenter);
        connect(check, &QCheckBox::toggled, this, [this, path = row.path](bool checked) {
            if (checked) _skipped.remove(path); else _skipped.insert(path);
            schedulePreview();
        });
        _table->setIndexWidget(_model->index(index, 0), checkArea);
        auto* target = new ElaComboBox(_table);
        QString automatic = row.create ? "新建：" + row.properties.value("姓名").toString() : "自动匹配（请选择对应成员）";
        for (int i = 0; i < _members.size(); ++i)
            if (_members[i].id == row.targetId) automatic = "匹配：" + memberLabel(_members[i], i);
        target->addItem(automatic, QString{});
        if (_mode->currentIndex() == 0 && row.properties.contains("姓名"))
            target->addItem("新建：" + row.properties.value("姓名").toString(), "new");
        for (int i = 0; i < _members.size(); ++i) target->addItem(memberLabel(_members[i], i), _members[i].id.toString());
        const int chosen = target->findData(_targets.value(row.path));
        target->setCurrentIndex(qMax(0, chosen));
        target->setEnabled(row.canChooseMember);
        connect(target, &QComboBox::activated, this, [this, target, path = row.path](int i) {
            _targets[path] = target->itemData(i).toString();
            schedulePreview();
        });
        _table->setIndexWidget(_model->index(index, 2), target);
        _table->setRowHeight(index, 42);
        if (!row.error.isEmpty()) ++problems;
        if (!row.selected && row.error.isEmpty()) hasUnselectedValidRow = true;
        if (row.selected) {
            ++selected;
            if (row.create) ++created;
            if (!row.error.isEmpty()) blocked = true;
        }
    }
    _displaying = false;
    updateThemeColors();
    _table->setEnabled(true);
    _summary->setText(_rows.isEmpty() ? QString{}
        : QString("%1 个文件 · 已勾选 %2 个（新建 %3 位，更新 %4 位） · %5 项待处理。只导入勾选项。")
            .arg(_rows.size()).arg(selected).arg(created).arg(selected - created).arg(problems));
    const bool missingSemester = (!_semesterStart.isValid() || _semesterStart.dayOfWeek() != 1)
        && std::any_of(_sources.cbegin(), _sources.cend(), [this](const auto& file) {
            return file.error.isEmpty() && file.root["dataType"].toString() == "course_schedule" && !_skipped.contains(file.path);
        });
    if (missingSemester)
        _summary->setText(_summary->text() + "\n下一步：请先选择学期第一周的周一，生成教务课表日期后再导入。");
    if (!_notice.isEmpty()) _summary->setText(_summary->text() + "\n" + _notice);
    _summary->setVisible(!_summary->text().isEmpty());
    const bool cancelAll = selected > 0 && !hasUnselectedValidRow;
    _toggleSelection->setText(cancelAll ? "全取消" : "全选");
    _toggleSelection->setProperty("cancelAll", cancelAll);
    _toggleSelection->setEnabled(selected > 0 || hasUnselectedValidRow);
    _import->setText(QString("确认导入 %1 项").arg(selected));
    _import->setEnabled(selected > 0 && !blocked);
}

void BatchRschImportDialog::toggleSelection() {
    if (!_toggleSelection->isEnabled()) return;
    if (_toggleSelection->property("cancelAll").toBool()) {
        for (const auto& path : _paths) _skipped.insert(path);
    } else {
        for (const auto& path : _paths) _skipped.remove(path);
    }
    schedulePreview();
}

void BatchRschImportDialog::updateThemeColors() {
    const auto mode = eTheme->getThemeMode();
    for (auto* combo : findChildren<ElaComboBox*>()) {
        auto palette = combo->palette();
        for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            const auto color = group == QPalette::Disabled ? ElaThemeColor(mode, BasicTextDisable) : ElaThemeColor(mode, BasicText);
            for (const auto role : {QPalette::Text, QPalette::ButtonText, QPalette::WindowText, QPalette::HighlightedText})
                palette.setColor(group, role, color);
        }
        combo->setPalette(palette);
        combo->view()->setPalette(palette);
        if (combo->lineEdit()) combo->lineEdit()->setPalette(palette);
    }
}

bool BatchRschImportDialog::confirmImport(const QList<RschImport::Row>& rows, const RschImport::Options& config) {
    int selected = 0, created = 0, courses = 0, scheduleOnly = 0;
    QSet<QString> newProperties;
    const auto existingProperties = _controller->getAllPropertyNames();
    for (const auto& row : rows) {
        if (!row.selected) continue;
        ++selected;
        if (row.scheduleOnly) ++scheduleOnly;
        if (row.create) ++created;
        for (const auto& source : _sources)
            if (source.path == row.path && source.root["dataType"].toString() == "course_schedule") ++courses;
        for (auto p = row.properties.cbegin(); p != row.properties.cend(); ++p)
            if (!existingProperties.contains(p.key())) newProperties.insert(p.key());
    }
    ImportConfirmationDialog confirmation(this);
    confirmation.setObjectName("rschImportConfirmation");
    confirmation.setWindowTitle("确认批量导入");
    auto* layout = new QVBoxLayout;
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(16);
    auto* heading = new ElaText("请核对本次导入", 21, &confirmation);
    heading->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(heading);
    auto* form = new QFormLayout;
    form->setVerticalSpacing(12);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto addSummary = [&](const QString& title, const QString& text, const QString& name) {
        auto* value = new ElaText(text, 15, &confirmation);
        value->setObjectName(name);
        value->setTextFormat(Qt::PlainText);
        value->setWordWrap(true);
        value->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        auto* label = new ElaText(title, 15, &confirmation);
        label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        form->addRow(label, value);
    };
    addSummary("导入数量：", QString("%1 份文件：新建 %2 位成员，更新 %3 位成员；跳过 %4 份")
        .arg(selected).arg(created).arg(selected - created).arg(_paths.size() - selected), "rschConfirmationCounts");
    addSummary("文件名格式：", config.format.simplified(), "rschConfirmationFormat");
    addSummary("导入方式：", config.existingOnly ? "仅更新已有成员" : "匹配已有成员，缺失时新建", "rschConfirmationMode");
    addSummary("学期起点：", courses > 0
        ? config.semesterStart.toString("yyyy-MM-dd") + QString("（第一周周一，用于 %1 份教务课表）").arg(courses)
        : "无需设置：所选文件均使用文件内的具体日期", "rschConfirmationSemester");
    if (courses > 0 && courses < selected)
        addSummary("普通时间表：", QString("其余 %1 份使用文件内日期，不按学期起点换算").arg(selected - courses), "rschConfirmationDates");
    if (scheduleOnly > 0)
        addSummary("手动对应：", QString("%1 份文件名无法解析，已手动选择成员；仅导入时间表，成员属性保持原值")
            .arg(scheduleOnly), "rschConfirmationScheduleOnly");
    addSummary("时间表处理：", config.replaceSchedule
        ? "替换：清空所选成员的整个原时间表，再导入文件中的时间表"
        : "合并：保留原有不可值班时间，补充导入文件中的时间", "rschConfirmationSchedulePolicy");
    addSummary("属性处理：", config.overwriteProperties
        ? "覆盖文件名中包含的其他属性；已有成员的姓名保持原值"
        : "仅补充空属性，保留已有内容和已有成员的姓名", "rschConfirmationAttributePolicy");
    auto properties = newProperties.values();
    properties.sort();
    addSummary("新增属性：", properties.isEmpty() ? "无" : properties.join("、"), "rschConfirmationNewProperties");
    layout->addLayout(form);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancel = new ElaPushButton("取消", &confirmation);
    cancel->setObjectName("rschImportCancelConfirmation");
    auto* acceptButton = new ElaPushButton("确定导入", &confirmation);
    acceptButton->setObjectName("rschImportAccept");
    buttons->addWidget(cancel);
    buttons->addWidget(acceptButton);
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, &confirmation, &QDialog::reject);
    connect(acceptButton, &QPushButton::clicked, &confirmation, &QDialog::accept);
    confirmation.setCentralLayout(layout, 680, 460);
    // ElaDialog applies its title-bar margins when shown; measure after that layout pass.
    QTimer::singleShot(0, &confirmation, [&confirmation] {
        auto* outerLayout = confirmation.layout();
        outerLayout->invalidate();
        const int contentHeight = outerLayout->hasHeightForWidth()
            ? outerLayout->totalHeightForWidth(confirmation.width()) : outerLayout->totalSizeHint().height();
        confirmation.setFixedHeight(contentHeight);
    });
    return confirmation.exec() == QDialog::Accepted;
}

void BatchRschImportDialog::importSelected() {
    if (!_import->isEnabled() || _reading) return;
    const auto rows = _rows;
    const auto revision = _revision;
    const auto generation = _generation;
    const auto config = options();
    _notice.clear();
    _import->setEnabled(false);
    if (!confirmImport(rows, config)) {
        if (generation == _generation && revision == _controller->importRevision()) _import->setEnabled(true);
        return;
    }
    if (generation != _generation || revision != _controller->importRevision()) {
        _notice = "尚未导入：预览或成员数据已变化，请重新核对并确认。";
        schedulePreview();
        return;
    }
    _applying = true;
    QString error;
    if (!_controller->applyRschImport(rows, revision, error)) {
        _applying = false;
        _notice = "尚未导入：" + error;
        schedulePreview();
        return;
    }
    Settings::instance()->setRschImportFormat(config.format.simplified());
    if (config.semesterStart.isValid() && config.semesterStart.dayOfWeek() == 1)
        Settings::instance()->setRschSemesterStart(config.semesterStart);
    accept();
}

void BatchRschImportDialog::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls() && !_reading) event->acceptProposedAction();
}

void BatchRschImportDialog::dropEvent(QDropEvent* event) {
    QStringList paths;
    for (const auto& url : event->mimeData()->urls()) if (url.isLocalFile()) paths.append(url.toLocalFile());
    addFiles(paths);
    event->acceptProposedAction();
}
