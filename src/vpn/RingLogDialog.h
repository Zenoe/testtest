#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QLabel;
class QPlainTextEdit;
class QPushButton;

class RingLogDialog final : public QDialog {
    Q_OBJECT

public:
    explicit RingLogDialog(const QString& configPath, QWidget* parent = nullptr);

private slots:
    void refresh();
    void copyAll();

private:
    void applyResult(bool ok, const QStringList& lines, const QString& detail);

    QPlainTextEdit* m_logView = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QString m_configPath;
};
