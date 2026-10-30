#include "aboutpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "ElaText.h"
#include "ElaScrollPageArea.h"
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>
#include <QDir>

AboutPage::AboutPage(QWidget* parent)
	: ElaScrollPage(parent)
{
    QWidget* container = new QWidget(this);
    container->setWindowTitle("关于");
    setTitleVisible(false);
    this->addCentralWidget(container);
    QVBoxLayout* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 20, 20);

    mainLayout->addWidget(new ElaText("<b>RabbitRoll</b>", 30));
    
    //一行高度为40-50
    ElaScrollPageArea* area1 = new ElaScrollPageArea(container);
    area1->setFixedHeight(140);
    QVBoxLayout* basis = new QVBoxLayout(area1);

    QHBoxLayout* version = new QHBoxLayout();
    version->addWidget(new ElaText("<b>版本：</b>", 20));
    version->addWidget(new ElaText("1.0.0", 18));
    version->addStretch();
    basis->addLayout(version);

    QHBoxLayout* date = new QHBoxLayout();
    date->addWidget(new ElaText("<b>构建日期：</b>", 20));
    date->addWidget(new ElaText("2026-04-28", 18));
    date->addStretch();
    basis->addLayout(date);

    QHBoxLayout* copyright = new QHBoxLayout();
    copyright->addWidget(new ElaText("<b>版权信息：</b>", 20));
    ElaText* t1 = new ElaText("© 2026 Ireiium. 保留所有权利", 18);
    t1->setWordWrap(false);
    copyright->addWidget(t1);
    copyright->addStretch();
    basis->addLayout(copyright);

    ElaScrollPageArea* area2 = new ElaScrollPageArea(container);
    area2->setFixedHeight(600);
    QVBoxLayout* license = new QVBoxLayout(area2);

    license->addWidget(new ElaText("<b>第三方许可说明</b>", 20));
    license->addSpacing(10);
    ElaText* t21 = new ElaText("<b>Qt(6.8.3):</b>", 18);
	QVBoxLayout* qtinfo = new QVBoxLayout();
	qtinfo->setContentsMargins(20, 0, 0, 0);
    ElaText* t22 = new ElaText("项目地址：<a href='https://www.qt.io' style='color:#0078d4; text-decoration:none;'>www.qt.io</a>", 18);
    t22->setOpenExternalLinks(true);
    t22->setWordWrap(false);
    ElaText* t23 = new ElaText("许可证: GNU Lesser General Public License v3.0", 18);
    t23->setWordWrap(false);
    ElaText* t24 = new ElaText("Qt库遵循LGPL v3,为满足该许可证要求，本软件以动态链接方式使用Qt库。您有权在遵守LGPL v3协议的前提下，通过替换本软件目录下的 Qt动态链接库文件，以实现测试、升级或修改的目的。您可以访问Qt官网获取完整源码。", 18);
    t24->setWordWrap(true);
    ElaText* t31 = new ElaText("<b>ElaWidgetTools:</b>", 18);
	QVBoxLayout* elainfo = new QVBoxLayout();
	elainfo->setContentsMargins(20, 0, 0, 0);
    ElaText* t32 = new ElaText("项目地址：<a href='https://github.com/Liniyous/ElaWidgetTools' style='color:#0078d4; text-decoration:none;'>https://github.com/Liniyous/ElaWidgetTools</a>", 18);
    t32->setOpenExternalLinks(true);
    ElaText* t33 = new ElaText("许可证: MIT License Copyright (c) 2024 Liniyous", 18);
    ElaText* t41 = new ElaText("<b>QXlsx:</b>", 18);
	QVBoxLayout* qxinfo = new QVBoxLayout();
	qxinfo->setContentsMargins(20, 0, 0, 0);
    ElaText* t42 = new ElaText("项目地址：<a href='https://github.com/QtExcel/QXlsx' style='color:#0078d4; text-decoration:none;'>https://github.com/QtExcel/QXlsx</a>", 18);
    t42->setOpenExternalLinks(true);
    ElaText* t43 = new ElaText("许可证: MIT License Copyright 2017-, <a href='https://github.com/j2doll/QXlsx' style='color:#0078d4; text-decoration:none;'>https://github.com/j2doll/QXlsx</a>", 18);
    t43->setOpenExternalLinks(true);

    ElaText* t51 = new ElaText("<b>Pybind11:</b>", 18);
    QVBoxLayout* pyinfo = new QVBoxLayout();
    pyinfo->setContentsMargins(20, 0, 0, 0);
    ElaText* t52 = new ElaText("项目地址：<a href='https://github.com/pybind/pybind11' style='color:#0078d4; text-decoration:none;'>https://github.com/pybind/pybind11</a>", 18);
    t52->setOpenExternalLinks(true);
    ElaText* t53 = new ElaText("许可证: BSD-style License Copyright (c) 2016 Wenzel Jakob <wenzel.jakob@epfl.ch>", 18);

    QHBoxLayout* footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(0, 10, 10, 10);
    footerLayout->addStretch();
    ElaText* licenseLink = new ElaText("<a href=\"open_licenses_folder\" style=\"color:#0078d4; text-decoration:none;\">查看许可证全文</a>", 14);
    licenseLink->setCursor(Qt::PointingHandCursor);
    connect(licenseLink, &ElaText::linkActivated, this, [=](const QString& link) {
        if (link == "open_licenses_folder") {
            QString path = QCoreApplication::applicationDirPath() + "/licenses";
            QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::toNativeSeparators(path)));
        }
        });

    license->addWidget(t21);
    qtinfo->addWidget(t22);
    qtinfo->addWidget(t23);
    qtinfo->addWidget(t24);
	license->addLayout(qtinfo);
    license->addSpacing(20);
    license->addWidget(t31);
    elainfo->addWidget(t32);
    elainfo->addWidget(t33);
    license->addLayout(elainfo);
    license->addSpacing(20);
    license->addWidget(t41);
    qxinfo->addWidget(t42);
    qxinfo->addWidget(t43);
	license->addLayout(qxinfo);
    license->addSpacing(20);
    license->addWidget(t51);
    pyinfo->addWidget(t52);
    pyinfo->addWidget(t53);
    license->addLayout(pyinfo);
	footerLayout->addWidget(licenseLink);
	license->addLayout(footerLayout);

    ElaScrollPageArea* area3 = new ElaScrollPageArea(container);
    area3->setFixedHeight(120);
    QVBoxLayout* credit = new QVBoxLayout(area3);

    credit->addWidget(new ElaText("<b>致谢</b>", 20));
    ElaText* wit = new ElaText("参与测试与反馈的用户", 18);
    credit->addWidget(wit);

    ElaScrollPageArea* area4 = new ElaScrollPageArea(container);
    area4->setFixedHeight(165);
    QVBoxLayout* info = new QVBoxLayout(area4);
    
    info->addWidget(new ElaText("<b>开发者信息</b>", 20));
    info->addWidget(new ElaText("Ireiium", 18));
    QHBoxLayout* myinfo = new QHBoxLayout();
    ElaText* githubTitle = new ElaText("GitHub 主页：", 18);
    githubTitle->setIsWrapAnywhere(false);
    githubTitle->setWordWrap(false);
    myinfo->addWidget(githubTitle);
    ElaText* t8 = new ElaText("<a href='https://github.com/Ireiium' style='color:#0078d4; text-decoration:none;'>https://github.com/Ireiium</a>", 18);
    t8->setOpenExternalLinks(true);
    t8->setIsWrapAnywhere(false);
    t8->setWordWrap(false);
    t8->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    myinfo->addWidget(t8);
    myinfo->addStretch();
    info->addLayout(myinfo);

    QHBoxLayout* repositoryInfo = new QHBoxLayout();
    ElaText* repositoryTitle = new ElaText("项目代码仓库：", 18);
    repositoryTitle->setIsWrapAnywhere(false);
    repositoryTitle->setWordWrap(false);
    repositoryInfo->addWidget(repositoryTitle);
    ElaText* repositoryLink = new ElaText("<a href='https://github.com/Ireiium/RabbitRoll' style='color:#0078d4; text-decoration:none;'>https://github.com/Ireiium/RabbitRoll</a>", 18);
    repositoryLink->setOpenExternalLinks(true);
    repositoryLink->setIsWrapAnywhere(false);
    repositoryLink->setWordWrap(false);
    repositoryLink->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    repositoryInfo->addWidget(repositoryLink);
    repositoryInfo->addStretch();
    info->addLayout(repositoryInfo);
    info->addStretch();

    mainLayout->addWidget(area1);
    mainLayout->addWidget(area2);
    mainLayout->addWidget(area3);
    mainLayout->addWidget(area4);

    mainLayout->addWidget(new ElaText("", 18));
    mainLayout->addWidget(new ElaText("", 18));
    mainLayout->addWidget(new ElaText("", 18));
    mainLayout->addWidget(new ElaText("", 18));
    mainLayout->addWidget(new ElaText("", 18));
}

AboutPage::~AboutPage()
{}

