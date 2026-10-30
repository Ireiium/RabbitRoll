#include "feishuexportdialog.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include "ElaComboBox.h"
#include "ElaPushButton.h"
#include "ElaSpinBox.h"
#include "exceltemplates.h"

FeishuExportDialog::FeishuExportDialog(const QString& outputPath,
    const QList<FeishuDutySlot>& duties, const QDate& month, QWidget* parent)
    : DialogAction(parent), _outputPath(outputPath), _duties(duties)
{
    setWindowTitle("导出飞书排班表");
    auto* layout = new QVBoxLayout;
    auto* hint = new QLabel("保存位置已确定。使用“设置 → 导出设置”中选择的 Excel 模板。\n"
                           "按姓名匹配原有成员，保留模板的 UID、表头、说明和班次选项。", this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* output = new QLabel("导出到：" + QDir::toNativeSeparators(outputPath), this);
    output->setObjectName("feishuOutputPath");
    output->setTextFormat(Qt::PlainText);
    output->setWordWrap(true);
    layout->addWidget(output);
    auto* templateRow = new QHBoxLayout;
    _templateFile = new QLabel("尚未选择飞书模板", this);
    _templateFile->setObjectName("feishuTemplateFile");
    _templateFile->setTextFormat(Qt::PlainText);
    _templateFile->setWordWrap(true);
    templateRow->addWidget(_templateFile, 1);
    layout->addLayout(templateRow);
    auto* form = new QFormLayout;
    _sheet = new ElaComboBox(this);
    _sheet->setObjectName("feishuTemplateSheet");
    form->addRow("排班工作表：", _sheet);
    _headerRow = new ElaSpinBox(this);
    _headerRow->setObjectName("feishuHeaderRow");
    _headerRow->setRange(1, 50);
    _headerRow->setValue(4);
    form->addRow("模板表头所在行：", _headerRow);
    _month = new QDateEdit(this);
    _month->setObjectName("feishuTemplateMonth");
    _month->setDisplayFormat("yyyy-MM");
    _month->setCalendarPopup(true);
    _month->setDate(month.isValid() ? month : QDate::currentDate());
    form->addRow("模板月份（日号列使用）：", _month);
    _templateDatesOnly = new QCheckBox("仅导出模板日期范围内的排班", this);
    _templateDatesOnly->setObjectName("feishuTemplateDatesOnly");
    _templateDatesOnly->setChecked(true);
    form->addRow({}, _templateDatesOnly);
    layout->addLayout(form);

    _readButton = new ElaPushButton("读取模板并预览", this);
    _readButton->setObjectName("feishuReadTemplate");
    layout->addWidget(_readButton);
    _status = new QLabel(this);
    _status->setObjectName("feishuTemplateStatus");
    _status->setWordWrap(true);
    layout->addWidget(_status);
    _mapping = new QTableWidget(0, 2, this);
    _mapping->setObjectName("feishuShiftMapping");
    _mapping->setHorizontalHeaderLabels({"本程序时段组合", "飞书班次名称 / 标记"});
    _mapping->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    _mapping->verticalHeader()->hide();
    _mapping->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(_mapping, 1);
    auto* note = new QLabel("填写飞书中已经创建的班次名称，或复制模板下拉框中的完整选项。\n"
                           "同一人同一天有多个时段时，请为整个组合指定一个对应班次。\n"
                           "周模式按模板日期展开；未排班的模板单元格保留原值。", this);
    note->setWordWrap(true);
    layout->addWidget(note);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    _exportButton = new ElaPushButton("导出", this);
    _exportButton->setObjectName("feishuExportConfirm");
    auto* cancel = new ElaPushButton("取消", this);
    buttons->addWidget(_exportButton);
    buttons->addWidget(cancel);
    layout->addLayout(buttons);
    setCentralLayout(layout, 780, 660);

    connect(_readButton, &QPushButton::clicked, this, &FeishuExportDialog::loadTemplate);
    connect(_sheet, &QComboBox::currentTextChanged, this, &FeishuExportDialog::invalidatePreview);
    connect(_headerRow, &QSpinBox::valueChanged, this, &FeishuExportDialog::invalidatePreview);
    connect(_month, &QDateEdit::dateChanged, this, &FeishuExportDialog::invalidatePreview);
    connect(_templateDatesOnly, &QCheckBox::toggled, this, &FeishuExportDialog::invalidatePreview);
    connect(_mapping, &QTableWidget::itemChanged, this, &FeishuExportDialog::updateExportButton);
    connect(_exportButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    loadTemplate();
}

void FeishuExportDialog::loadTemplate()
{
    QString error;
    const auto path = ExcelTemplates::selectedFilePath(error);
    if (path != _templatePath) _mapping->setRowCount(0);
    _templatePath = path;
    _templateFile->setText(path.isEmpty() ? "模板尚未就绪，请在设置中选择" : "模板：" + QFileInfo(path).fileName());
    _templateFile->setToolTip(path);
    const auto sheets = error.isEmpty() ? FeishuScheduleExporter::sheetNames(path, error) : QStringList{};
    const auto previousSheet = _sheet->currentText();
    _sheet->blockSignals(true);
    _sheet->clear();
    _sheet->addItems(sheets);
    if (sheets.contains(previousSheet)) _sheet->setCurrentText(previousSheet);
    _sheet->blockSignals(false);
    if (!error.isEmpty()) {
        _previewValid = false;
        _mapping->setRowCount(0);
        _status->setText(error);
        _status->setStyleSheet("color: #b42318;");
        updateExportButton();
        return;
    }
    updatePreview();
}

FeishuExportRequest FeishuExportDialog::exportRequest() const
{
    FeishuExportRequest request;
    request.outputPath = _outputPath;
    request.settings.filePath = _templatePath;
    request.settings.sheetName = _sheet->currentText();
    request.settings.headerRow = _headerRow->value();
    request.settings.month = _month->date();
    request.settings.templateDatesOnly = _templateDatesOnly->isChecked();
    request.duties = _duties;
    for (int row = 0; row < _mapping->rowCount(); ++row) {
        if (_mapping->item(row, 0) && _mapping->item(row, 1))
            request.shiftMapping.insert(_mapping->item(row, 0)->text(), _mapping->item(row, 1)->text().trimmed());
    }
    return request;
}

void FeishuExportDialog::updatePreview()
{
    const auto request = exportRequest();
    FeishuTemplateInfo info;
    QString error;
    _previewValid = FeishuScheduleExporter::inspect(request.settings, _duties, info, error);
    _mapping->blockSignals(true);
    _mapping->setRowCount(0);
    if (_previewValid) {
        _mapping->setRowCount(info.shiftCombinations.size());
        for (int row = 0; row < info.shiftCombinations.size(); ++row) {
            const auto& key = info.shiftCombinations[row];
            auto* combination = new QTableWidgetItem(key);
            combination->setFlags(combination->flags() & ~Qt::ItemIsEditable);
            _mapping->setItem(row, 0, combination);
            _mapping->setItem(row, 1, new QTableWidgetItem(request.shiftMapping.value(key)));
        }
        _status->setText(QString("已识别 %1 个日期，将回填 %2 个人员日期单元格。请填写下方班次对应关系。")
                         .arg(info.dateColumns.size()).arg(info.cellCount));
        if (info.skippedDateSlots > 0)
            _status->setText(_status->text() + QString("\n模板范围外的 %1 条具体日期排班不会导出。").arg(info.skippedDateSlots));
        _status->setStyleSheet({});
    } else {
        _status->setText(error);
        _status->setStyleSheet("color: #b42318;");
    }
    _mapping->blockSignals(false);
    updateExportButton();
}

void FeishuExportDialog::invalidatePreview()
{
    _previewValid = false;
    _status->setText("模板设置已变更，请点击“读取模板并预览”。");
    updateExportButton();
}

void FeishuExportDialog::updateExportButton()
{
    bool ready = _previewValid && _mapping->rowCount() > 0;
    for (int row = 0; row < _mapping->rowCount(); ++row) {
        if (!_mapping->item(row, 1) || _mapping->item(row, 1)->text().trimmed().isEmpty()) ready = false;
    }
    _exportButton->setEnabled(ready);
}
