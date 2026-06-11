#include "TerminalRegistrationDialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

TerminalRegistrationDialog::TerminalRegistrationDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("终端注册"));
    setModal(true);
    setFixedWidth(420);

    auto* title = new QLabel(tr("终端尚未注册"), this);
    title->setObjectName("LoginTitle");

    auto* subtitle = new QLabel(tr("请输入管理员提供的注册验证码后继续登录"), this);
    subtitle->setObjectName("LoginSubtitle");
    subtitle->setWordWrap(true);

    m_codeEdit = new QLineEdit(this);
    m_codeEdit->setPlaceholderText(tr("注册验证码"));
    m_codeEdit->setClearButtonEnabled(true);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("StatusLabel");
    m_statusLabel->setProperty("ok", false);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->hide();

    m_submitButton = new QPushButton(tr("注册并继续"), this);
    m_submitButton->setObjectName("PrimaryButton");
    m_submitButton->setDefault(true);

    auto* cancelButton = new QPushButton(tr("取消登录"), this);
    cancelButton->setObjectName("SecondaryButton");

    connect(m_submitButton, &QPushButton::clicked, this, &TerminalRegistrationDialog::submit);
    connect(m_codeEdit, &QLineEdit::returnPressed, this, &TerminalRegistrationDialog::submit);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addSpacing(6);
    layout->addWidget(m_codeEdit);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_submitButton);
    layout->addWidget(cancelButton);
}

void TerminalRegistrationDialog::setSubmitting(bool submitting)
{
    m_codeEdit->setEnabled(!submitting);
    m_submitButton->setEnabled(!submitting);
    m_submitButton->setText(submitting ? tr("正在注册...") : tr("注册并继续"));
}

void TerminalRegistrationDialog::showError(const QString& message)
{
    m_statusLabel->setText(message);
    m_statusLabel->show();
}

void TerminalRegistrationDialog::submit()
{
    const QString code = m_codeEdit->text().trimmed();
    if (code.isEmpty()) {
        showError(tr("请输入注册验证码"));
        return;
    }

    setSubmitting(true);
    m_statusLabel->hide();
    emit registrationRequested(code);
}
