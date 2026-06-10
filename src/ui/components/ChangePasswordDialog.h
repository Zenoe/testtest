#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

class ChangePasswordDialog : public QDialog {
    Q_OBJECT

public:
    explicit ChangePasswordDialog(QWidget* parent = nullptr);

private:
    void submit();
    void setSubmitting(bool submitting);
    void showStatus(const QString& message, bool ok);

    QLineEdit* m_oldPasswordEdit;
    QLineEdit* m_newPasswordEdit;
    QLineEdit* m_confirmPasswordEdit;
    QLabel* m_statusLabel;
    QPushButton* m_submitButton;
};
