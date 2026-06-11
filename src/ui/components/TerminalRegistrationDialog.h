#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

class TerminalRegistrationDialog : public QDialog {
    Q_OBJECT

public:
    explicit TerminalRegistrationDialog(QWidget* parent = nullptr);

    void setSubmitting(bool submitting);
    void showError(const QString& message);

signals:
    void registrationRequested(const QString& code);

private:
    void submit();

    QLineEdit* m_codeEdit;
    QLabel* m_statusLabel;
    QPushButton* m_submitButton;
};
