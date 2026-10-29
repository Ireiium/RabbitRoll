#pragma once

#include <ElaDockWidget.h>
#include <QModelIndex>
#include <QUuid>

class ElaProgressRing;
class ElaScrollArea;
class ElaCheckBox;
class ElaText;
class QStackedWidget;
class QVBoxLayout;
class EditorDutyController;
class MemberInfoController;

class EditorSelectDock : public ElaDockWidget
{
    Q_OBJECT
public:
    explicit EditorSelectDock(QWidget* parent = nullptr);
    ~EditorSelectDock();

    void setTargetCell(const QModelIndex& index, bool isCalculating);
    void onCalculationFinished();

private:
    void initUi();
    void clearListLayout();
    void updateList();

    EditorDutyController* _eController = nullptr;
    MemberInfoController* _mController = nullptr;

    QModelIndex _currentIndex;
    QStackedWidget* _stack = nullptr;
    ElaProgressRing* _busyRing = nullptr;
    QVBoxLayout* _listLayout = nullptr;
};