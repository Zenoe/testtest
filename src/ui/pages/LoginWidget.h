#pragma once
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QProgressBar>

#include "ui/components/ClickableLabel.h"
class LoginWidget : public QWidget {
    Q_OBJECT

public:
    explicit LoginWidget(QWidget* parent = nullptr);

    void setLoading(bool on);
    void showError(const QString& msg);
    void clearFields();

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent *event);
signals:
    //void loginRequested(const QString& username, const QString& password);
    void loginRequested(const QString& username,
        const QString& password,
        const QString& captchaCode,
        const QString& captchaUuid);
    void logoutRequested();        // emitted if already-logged-in state needs reset

private slots:
    void onLoginClicked();
    void onFieldsChanged();

    void onCaptchaFetched(bool success,
        const QString& imgBase64,
        const QString& uuid,
        bool captchaEnabled,
        const QString& errorMsg);

    void refreshCaptcha();   // 点击图片刷新用
private:
    void setupUi();
    void setupConnections();

    QWidget* m_formPanel = nullptr;
    /* void setCentralBackground(); */
    // Left panel
    QLabel*       m_bgLabel;       // background image
    /* QWidget* m_overlayContainer = nullptr; */
    // Right panel – form
    QLabel*       m_titleLabel;
    QLabel*       m_errorLabel;
    QLineEdit*    m_userEdit;
    QLineEdit*    m_passEdit;
    QPushButton*  m_loginBtn;
    QCheckBox*    m_autoLoginChk;
    QLabel*       m_forgotLabel;
    QProgressBar* m_spinner;       // indeterminate, shown while authenticating

    ClickableLabel* m_captchaImageLabel = nullptr;
    QLineEdit* m_captchaEdit = nullptr;

    QString m_captchaUuid;
    bool    m_captchaEnabled = false;
    QString m_captchaBaseUrl;        // 保存完整验证码接口地址
};
