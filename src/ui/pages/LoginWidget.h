#pragma once
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QProgressBar>

class LoginWidget : public QWidget {
    Q_OBJECT

public:
    explicit LoginWidget(QWidget* parent = nullptr);

    void setLoading(bool on);
    void showError(const QString& msg);
    void clearFields();

protected:
    void resizeEvent(QResizeEvent *event);
signals:
    void loginRequested(const QString& username, const QString& password);
    void logoutRequested();        // emitted if already-logged-in state needs reset

private slots:
    void onLoginClicked();
    void onFieldsChanged();

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
};
