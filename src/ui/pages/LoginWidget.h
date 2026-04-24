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
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    //void loginRequested(const QString& username, const QString& password);
    void loginRequested(const QString& username,
        const QString& password,
        const QString& captchaCode,
        const QString& captchaUuid);
    void loginSuccess(const QString token);        // emitted if already-logged-in state needs reset

private slots:
    void onLoginClicked();
    void onFieldsChanged();

    void onCaptchaFetched(bool success,
        const QString& imgBase64,
        const QString& uuid,
        bool captchaEnabled,
        const QString& errorMsg);

    void refreshCaptcha();   // 点击图片刷新用

    void startCaptchaLoading();
    void stopCaptchaLoading();
    void updateLoadingDots();
    void onLoginFinished(bool success, const QString& token, const QString& errorMsg);

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
    QCheckBox*    m_rememberMe;
    QLabel*       m_forgotLabel;
    QProgressBar* m_spinner;       // indeterminate, shown while authenticating

    ClickableLabel* m_captchaImageLabel = nullptr;
    QLineEdit* m_captchaEdit = nullptr;

    QString m_captchaUuid;
    bool    m_captchaEnabled = false;
    QString m_loginUrl;
private:
	bool    m_isFetchingCaptcha = false;   // 防重复点击
	QTimer* m_loadingTimer = nullptr;
	int     m_dotsCount = 0;
	QString getServerHost() ;
};
