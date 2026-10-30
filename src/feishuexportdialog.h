#pragma once

#include "dialogaction.h"
#include "feishuscheduleexporter.h"

class QComboBox;
class QDateEdit;
class QSpinBox;
class QLabel;
class QTableWidget;
class QPushButton;
class QCheckBox;

class FeishuExportDialog : public DialogAction
{
public:
    FeishuExportDialog(const QString& outputPath, const QList<FeishuDutySlot>& duties,
                       const QDate& month, QWidget* parent = nullptr);
    FeishuExportRequest exportRequest() const;

private:
    void loadTemplate();
    void updatePreview();
    void invalidatePreview();
    void updateExportButton();

    QString _outputPath;
    QString _templatePath;
    QList<FeishuDutySlot> _duties;
    QComboBox* _sheet;
    QDateEdit* _month;
    QSpinBox* _headerRow;
    QCheckBox* _templateDatesOnly;
    QLabel* _status;
    QLabel* _templateFile;
    QTableWidget* _mapping;
    QPushButton* _readButton;
    QPushButton* _exportButton;
    bool _previewValid{false};
};
