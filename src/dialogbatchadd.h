#pragma once

#include "ElaScrollPage.h"

class WinDialog;
class QStackedLayout;
class ElaPlainTextEdit;
class DialogBatchAdd  : public ElaScrollPage
{
	Q_OBJECT

friend class WinDialog;

Q_SIGNALS:
	void importFromFileRequested();
	void batchAddWithFormatRequested(const QStringList& lines, const QStringList& format);

public:
	DialogBatchAdd(QWidget *parent, WinDialog* dlg);
	~DialogBatchAdd();
	void setLoadingState(bool isLoading);
	void reset();

private:
	void updateFormatDisplay();
	QStackedLayout* _stackedLayout{ nullptr };
	ElaPlainTextEdit* _edit{ nullptr };
	QStringList _currentFormat;
	QWidget* _formatWidget{ nullptr };
	QHBoxLayout* _formatLayout{ nullptr };
};

