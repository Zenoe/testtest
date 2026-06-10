#include "ChangePasswordDialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStyle>
#include "backend/NetworkManager.h"
#include "backend/SessionManager.h"

ChangePasswordDialog::ChangePasswordDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("修改密码"));
    setModal(true);
    setFixedWidth(420);

    auto* title = new QLabel(tr("修改密码"), this);
    title->setObjectName("LoginTitle");

    auto* subtitle = new QLabel(tr("请输入当前密码并设置新密码"), this);
    subtitle->setObjectName("LoginSubtitle");

    m_oldPasswordEdit = new QLineEdit(this);
    m_oldPasswordEdit->setEchoMode(QLineEdit::Password);
    m_oldPasswordEdit->setPlaceholderText(tr("当前密码"));

    m_newPasswordEdit = new QLineEdit(this);
    m_newPasswordEdit->setEchoMode(QLineEdit::Password);
    m_newPasswordEdit->setPlaceholderText(tr("新密码"));

    m_confirmPasswordEdit = new QLineEdit(this);
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    m_confirmPasswordEdit->setPlaceholderText(tr("再次输入新密码"));

    auto* form = new QFormLayout;
    form->setSpacing(12);
    form->addRow(tr("当前密码:"), m_oldPasswordEdit);
    form->addRow(tr("新密码:"), m_newPasswordEdit);
    form->addRow(tr("确认新密码:"), m_confirmPasswordEdit);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("StatusLabel");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->hide();

    auto* cancelButton = new QPushButton(tr("取消"), this);
    cancelButton->setObjectName("SecondaryButton");
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    m_submitButton = new QPushButton(tr("确认修改"), this);
    m_submitButton->setObjectName("PrimaryButton");
    m_submitButton->setDefault(true);
    connect(m_submitButton, &QPushButton::clicked, this, &ChangePasswordDialog::submit);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(cancelButton);
    buttons->addWidget(m_submitButton);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addLayout(form);
    layout->addWidget(m_statusLabel);
    layout->addLayout(buttons);

    auto& network = NetworkManager::instance();
    connect(&network, &NetworkManager::passwordEditFinished, this,
        [this](bool success, const QString& message) {
            setSubmitting(false);
            showStatus(message, success);
            if (success) {
                m_oldPasswordEdit->clear();
                m_newPasswordEdit->clear();
                m_confirmPasswordEdit->clear();
            }
        });

    connect(&SessionManager::instance(), &SessionManager::sessionChanged, this, [this]() {
        if (!SessionManager::instance().isLoggedIn())
            reject();
    });
}

void ChangePasswordDialog::submit()
{
    if (!SessionManager::instance().isLoggedIn()) {
        showStatus(tr("请先登录后再修改密码"), false);
        return;
    }

    const QString oldPassword = m_oldPasswordEdit->text();
    const QString newPassword = m_newPasswordEdit->text();
    const QString confirmPassword = m_confirmPasswordEdit->text();

    if (oldPassword.isEmpty() || newPassword.isEmpty() || confirmPassword.isEmpty()) {
        showStatus(tr("请填写全部密码字段"), false);
        return;
    }
    if (newPassword != confirmPassword) {
        showStatus(tr("两次输入的新密码不一致"), false);
        return;
    }
    if (oldPassword == newPassword) {
        showStatus(tr("新密码不能与当前密码相同"), false);
        return;
    }

    setSubmitting(true);
    showStatus(tr("正在修改密码..."), true);
    NetworkManager::instance().editPassword(oldPassword, newPassword);
}

void ChangePasswordDialog::setSubmitting(bool submitting)
{
    m_oldPasswordEdit->setEnabled(!submitting);
    m_newPasswordEdit->setEnabled(!submitting);
    m_confirmPasswordEdit->setEnabled(!submitting);
    m_submitButton->setEnabled(!submitting);
}

void ChangePasswordDialog::showStatus(const QString& message, bool ok)
{
    m_statusLabel->setProperty("ok", ok);
    m_statusLabel->setText(message);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
    m_statusLabel->show();
}
