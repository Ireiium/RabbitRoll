#include "dialogpropertyedit.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include "ElaCheckBox.h"
#include <QButtonGroup>
#include "memberpage.h"
#include "windialog.h"
#include "componentmanager.h"

//控制【属性设置】的UI

DialogPropertyEdit::DialogPropertyEdit(QWidget* parent, WinDialog* dlg)
    : ElaScrollPage(parent)
{
    auto* comp = ComponentManager::instance()->getComponent<MemberInfoComponent>();
    _controller = comp->getController();

    setWindowTitle("属性设置");
    setTitleVisible(false);
    _centralWidget = new QWidget(this);
	_layout = new QVBoxLayout(_centralWidget);
	_boxLayout = new QGridLayout();
	_layout->addLayout(_boxLayout);
	_layout->addStretch();

	_group = new QButtonGroup(this);
    _group->setExclusive(false);

    this->addCentralWidget(_centralWidget);

    connect(dlg, &WinDialog::showProPage, this, [=]() {
        QList<QString> details = _controller->getAllPropertyNames();
		/*qDebug() << details;*/
        QList<QAbstractButton*>btns = _group->buttons();
        for (auto* btn :btns) {
            _boxLayout->removeWidget(btn);
            _group->removeButton(btn);
            btn->deleteLater();
        }
        for (int i = 0; i < details.size();++i) {
            /*qDebug() << details[i];*/
            ElaCheckBox* cb = new ElaCheckBox(details[i], _centralWidget);
            _group->addButton(cb);
            _boxLayout->addWidget(cb, i / 4, i % 4);

            //不允许删除名字这个属性
            if (details[i] == "姓名") {
                cb->setTristate(true);
                connect(cb, &ElaCheckBox::clicked, this, [cb]() {
                    Qt::CheckState cur = cb->checkState();
                    if (cur == Qt::Checked) {
                        cb->setCheckState(Qt::Unchecked);
                    }
                    else if (cur == Qt::PartiallyChecked) {
                    }
                    });
            }
        }
        });

    connect(dlg,&WinDialog::selectAll, this, [=]() {
        QList<QAbstractButton*>btns = _group->buttons();
        for (auto* btn :btns) {
            if (auto* cb = qobject_cast<ElaCheckBox*>(btn)) {
                if (btn->text() == "姓名") {
                    cb->setCheckState(Qt::PartiallyChecked);
                }
                else {
                    cb->setChecked(true);
                }
            }
		}
		});

    connect(dlg, &WinDialog::unSelectAll, this, [=]() {
        QList<QAbstractButton*>btns = _group->buttons();
        for (auto* btn :btns) {
            if (auto* cb = qobject_cast<ElaCheckBox*>(btn)) {
                cb->setChecked(false);
			}
		}
		});

    connect(dlg, &WinDialog::deleteChecked, this, [=]() {
        QList<QAbstractButton*>btns = _group->buttons();
        QList<QString> toDeleteList;
        QList<ElaCheckBox*> remainCheckBoxs;

        for (auto* btn :btns) {
            if (auto* cb = qobject_cast<ElaCheckBox*>(btn)) {
                if (cb->checkState() == Qt::Checked) {
                    toDeleteList << cb->text();
                }
                else {
                    remainCheckBoxs << cb;
                }
            }
        }

        if (toDeleteList.isEmpty()) return;

        for (const QString& attrName : toDeleteList) {
            _controller->removeProperty(attrName);
        }
        for (auto* cb : remainCheckBoxs) {
            _boxLayout->removeWidget(cb);
        }
        for (auto* btn :btns) {
            if (toDeleteList.contains(btn->text())) {
                _group->removeButton(btn);
                btn->deleteLater();
            }
        }
        for (int i = 0; i < remainCheckBoxs.size(); ++i) {
            _boxLayout->addWidget(remainCheckBoxs[i], i / 4, i % 4);
        }

        });
}

DialogPropertyEdit::~DialogPropertyEdit() {
}
