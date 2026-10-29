#pragma once

#include "ElaScrollPage.h"

class QVBoxLayout;
class ElaImageCard;

class HomePage : public ElaScrollPage
{
    Q_OBJECT
public:
    explicit HomePage(QWidget* parent = nullptr);
    bool eventFilter(QObject* watched, QEvent* event) override;

	void initialUI();
    void loadRecentFiles();

private:
    QVBoxLayout* _recentListLayout{ nullptr };
    ElaImageCard* _homepageCard{ nullptr };
    int _winMode;

Q_SIGNALS:
    void createNew();
    void openFile();
    void openRecentFile(const QString& filePath);
    void deleteRecentFile(const QString& filePath);//转发给mainwindow

public Q_SLOTS:
    void changeImage(int winMode);
};

