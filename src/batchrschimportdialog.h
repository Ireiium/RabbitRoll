#pragma once

#include "dialogaction.h"
#include "rschimport.h"

class MemberInfoController;
class ElaComboBox;
class ElaCalendarPicker;
class ElaPushButton;
class ElaTableView;
class QLabel;
class QStandardItemModel;
class QTimer;
class QDragEnterEvent;
class QDropEvent;

class BatchRschImportDialog : public DialogAction {
    Q_OBJECT
public:
    explicit BatchRschImportDialog(MemberInfoController* controller, QWidget* parent = nullptr);
    void addFiles(const QStringList& paths);
protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
private:
    RschImport::Options options() const;
    void readFiles();
    void schedulePreview();
    void rebuildPreview();
    void displayPreview();
    void importSelected();
    bool confirmImport(const QList<RschImport::Row>& rows, const RschImport::Options& config);
    void updateThemeColors();
    void toggleSelection();
    MemberInfoController* _controller;
    ElaComboBox* _format;
    ElaComboBox* _mode;
    ElaComboBox* _merge;
    ElaComboBox* _attributes;
    ElaCalendarPicker* _semester;
    ElaPushButton* _chooseFiles;
    ElaPushButton* _chooseFolder;
    ElaPushButton* _refresh;
    ElaPushButton* _toggleSelection;
    ElaPushButton* _import;
    ElaTableView* _table;
    QStandardItemModel* _model;
    QLabel* _summary;
    QLabel* _example;
    QLabel* _semesterHint;
    QTimer* _previewTimer;
    QDate _semesterStart;
    QString _notice;
    QStringList _paths;
    QList<RschImport::Source> _sources;
    QList<RschImport::Row> _rows;
    QList<MemberInfoModel::MemberData> _members;
    QMap<QString, QString> _targets;
    QSet<QString> _skipped;
    quint64 _generation = 0;
    quint64 _readGeneration = 0;
    quint64 _revision = 0;
    bool _reading = false;
    bool _displaying = false;
    bool _applying = false;
};
