#pragma once

#include "ElaScrollPageArea.h"

class ElaComboBox;
class QFileSystemWatcher;

class ExcelTemplatePicker : public ElaScrollPageArea
{
public:
    explicit ExcelTemplatePicker(QWidget* parent = nullptr);
    void reloadTemplates();

protected:
    void showEvent(QShowEvent* event) override;

private:
    ElaComboBox* _templates;
    QFileSystemWatcher* _watcher;
};
