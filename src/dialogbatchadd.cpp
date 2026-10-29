#include "dialogbatchadd.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "ElaPlainTextEdit.h"
#include "ElaText.h"
#include "ElaPushButton.h"
#include "windialog.h"
#include "ElaScrollPage.h"
#include "ElaProgressRing.h"
#include <QStackedLayout>
#include "dialogaction.h"
#include <QDialog>

DialogBatchAdd::DialogBatchAdd(QWidget* parent, WinDialog* dlg)
    : ElaScrollPage(parent)
{
    QWidget* centralWidget = new QWidget(this);
    this->addCentralWidget(centralWidget);
    setTitleVisible(false);

    _stackedLayout = new QStackedLayout(centralWidget);
    //index0
    QWidget* contentWidget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(contentWidget);
    QVBoxLayout* editLayout = new QVBoxLayout();
    layout->addLayout(editLayout, 2);
    QVBoxLayout* btnLayout = new QVBoxLayout();
    layout->addLayout(btnLayout, 1);
    _formatWidget = new QWidget(this);
    _formatLayout = new QHBoxLayout(_formatWidget);
    _formatLayout->setContentsMargins(0, 0, 0, 0);
    _formatWidget->setVisible(false);
    editLayout->addWidget(_formatWidget);
    _edit = new ElaPlainTextEdit(this);
    _edit->setPlaceholderText("请输入内容\n一行代表一个成员，如：\n张三\n李四\n……\n若要批量添加时候设置好成员各属性，点击【添加格式】");
    editLayout->addWidget(_edit);

    ElaPushButton* certainBtn = new ElaPushButton("确定", this);
    btnLayout->addWidget(certainBtn);
    ElaPushButton* setBtn = new ElaPushButton("添加格式", this);
    btnLayout->addWidget(setBtn);
    ElaPushButton* importBtn = new ElaPushButton("从文件导入", this);
    btnLayout->addWidget(importBtn);
    btnLayout->addStretch();

    //index1
    QWidget* loadingWidget = new QWidget(this);
    QVBoxLayout* loadingLayout = new QVBoxLayout(loadingWidget);
    loadingWidget->layout()->setContentsMargins(0, 0, 0, 0);
    ElaProgressRing* ring = new ElaProgressRing(this);
    ring->setIsBusying(true);
	ring->setMinimumSize(125, 125);
    loadingLayout->addSpacing(100);
    loadingLayout->addWidget(ring, 0, Qt::AlignCenter);
    loadingLayout->addStretch();
    _stackedLayout->addWidget(contentWidget);
    _stackedLayout->addWidget(loadingWidget);
    _stackedLayout->setCurrentIndex(0);

    connect(certainBtn, &ElaPushButton::clicked, this, [=]() {
        QString text = _edit->toPlainText();
        if (!text.isEmpty()) {
            _stackedLayout->setCurrentIndex(1);

            QStringList list = text.split('\n', Qt::SkipEmptyParts);

            Q_EMIT batchAddWithFormatRequested(list, _currentFormat);
        }
        else {
            dlg->close();
        }
        });

    connect(importBtn, &ElaPushButton::clicked, this, &DialogBatchAdd::importFromFileRequested);
    connect(setBtn, &ElaPushButton::clicked, this, [=]() {
        DialogAction formatDialog(this);
        formatDialog.setWindowTitle("设置解析格式");

        QVBoxLayout* contentLayout = new QVBoxLayout();

        ElaText* hintText = new ElaText("预设格式，属性间用空格隔开\n例如：姓名 部门 电话\n(注：未设置“姓名”将自动添加为首项)", 14);
        contentLayout->addWidget(hintText);

        ElaPlainTextEdit* formatInput = new ElaPlainTextEdit(&formatDialog);
        formatInput->setFixedHeight(80);
        if (!_currentFormat.isEmpty()) {
            formatInput->setPlainText(_currentFormat.join(" "));
        }
        contentLayout->addWidget(formatInput);

        QHBoxLayout* dialogBtnLay = new QHBoxLayout();
        dialogBtnLay->addStretch();
        ElaPushButton* okBtn = new ElaPushButton("确定");
        okBtn->setFixedSize(80, 40);
        ElaPushButton* cancelBtn = new ElaPushButton("取消");
        cancelBtn->setFixedSize(80, 40);
        dialogBtnLay->addWidget(okBtn);
        dialogBtnLay->addSpacing(10);
        dialogBtnLay->addWidget(cancelBtn);
        contentLayout->addLayout(dialogBtnLay);

        formatDialog.setCentralLayout(contentLayout, 380, 180);

        connect(okBtn, &ElaPushButton::clicked, &formatDialog, &QDialog::accept);
        connect(cancelBtn, &ElaPushButton::clicked, &formatDialog, &QDialog::reject);

        if (formatDialog.exec() == QDialog::Accepted) {
            QString rawText = formatInput->toPlainText().simplified();
            if (!rawText.isEmpty()) {
                _currentFormat = rawText.split(" ", Qt::SkipEmptyParts);

                _currentFormat.removeAll("姓名");
                _currentFormat.prepend("姓名");
            }
            else {
                _currentFormat.clear();
            }
            updateFormatDisplay();
        }
        });
}

DialogBatchAdd::~DialogBatchAdd()
{
}

void DialogBatchAdd::updateFormatDisplay() {
    QLayoutItem* item;
    while ((item = _formatLayout->takeAt(0)) != nullptr) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    if (_currentFormat.isEmpty()) {
        _formatWidget->setVisible(false);
        _edit->setPlaceholderText("请输入内容\n一行代表一个成员，如：\n张三\n李四\n……\n若要批量添加时候设置好成员各属性，点击【添加格式】");
        return;
    }
    _formatLayout->addWidget(new ElaText("当前解析格式：", 14));
    for (int i = 0; i < _currentFormat.size(); ++i) {
        ElaText* tag = new ElaText(QString("[%1]").arg(_currentFormat[i]), 14);
        if (i == 0) {
            tag->setStyleSheet("color: #0078D4; font-weight: bold;");
        }
        _formatLayout->addWidget(tag);
    }
    _formatLayout->addStretch();
    _formatWidget->setVisible(true);
    QString hint = "请输入内容，属性间用空格分隔。\n当前格式示例：\n" + _currentFormat.join(" ") + "\n张三";
    if (_currentFormat.size() > 1) {
        hint += " 摸鱼部";
    }
    if (_currentFormat.size() > 2) {
        hint += " 233";
    }
    _edit->setPlaceholderText(hint);
}

void DialogBatchAdd::setLoadingState(bool isLoading) {
    if (_stackedLayout) {
        _stackedLayout->setCurrentIndex(isLoading ? 1 : 0);
    }
}

void DialogBatchAdd::reset() {
    if (_edit) {
        _edit->clear();
    }
    _stackedLayout->setCurrentIndex(0);
    updateFormatDisplay();
}