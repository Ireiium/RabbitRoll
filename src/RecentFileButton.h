#pragma once

#include <QPushButton>
#include <QEnterEvent>
#include <QEvent>
#include <QPaintEvent>
#include <QContextMenuEvent>

class QLabel;

class RecentFileButton : public QPushButton
{
    Q_OBJECT

Q_SIGNALS:
    void deleteRequested(const QString& filePath); 
public:
    explicit RecentFileButton(const QString& filePath, QWidget* parent = nullptr);

    QString getFilePath() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QString _filePath;
    QLabel* _nameLabel;
    QLabel* _pathLabel;
    QLabel* _dateLabel;

    void openFileInLocation(const QString& filePath);
};
