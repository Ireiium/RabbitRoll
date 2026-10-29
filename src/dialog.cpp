#include "dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "ElaText.h"
#include "ElaLineEdit.h"
#include "ElaPushButton.h"

//控制所有二级对话框
//放弃了这种一个类分flag方式，后面的除了添加属性，都用DialogAction，直接在调用类中直接new或者创建。

Dialog::Dialog(RTypies::Dialog key, QWidget *parent)
	: ElaDialog(parent)
{
    this->setAttribute(Qt::WA_DeleteOnClose);
	this->setWindowModality(Qt::WindowModal);
	setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
	setWindowTitle("");
	this->setWindowIcon(QIcon(""));

	this->setIsFixedSize(true);
	

	ElaPushButton* certainBtn = new ElaPushButton("确定", this);

	ElaPushButton* cancelBtn = new ElaPushButton("取消", this);

	if (key == RTypies::Dialog::AddProperties) {
		this->setFixedSize(250, 150);
		setWindowTitle("添加属性");

		QVBoxLayout* layout = new QVBoxLayout(this);
		QHBoxLayout* editLayout = new QHBoxLayout();
		QHBoxLayout* btnLayout = new QHBoxLayout();
		layout->addLayout(editLayout);
		layout->addStretch();
		layout->addItem(btnLayout);

		ElaText* label = new ElaText("添加", this);
		label->setTextPointSize(14);
		ElaLineEdit* edit = new ElaLineEdit(this);
		editLayout->addWidget(label);
		editLayout->setSpacing(20);
		editLayout->addWidget(edit);

		btnLayout->addStretch();
		certainBtn->resize(100, 40);
		btnLayout->addWidget(certainBtn);
		cancelBtn->resize(100, 40);
		btnLayout->addStretch();
		btnLayout->addWidget(cancelBtn);
		btnLayout->addStretch();

		layout->addStretch();

		connect(certainBtn, &ElaPushButton::clicked, this, [=]() {
			Q_EMIT certainClicked_newProperty(edit->text());
			this -> close();
			});

		connect(cancelBtn, &ElaPushButton::clicked, this, [=]() {
			this->close();
			});
	}
}

Dialog::~Dialog()
{}

