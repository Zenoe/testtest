#include "LoginWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeyEvent>

#include "backend/NetworkManager.h"
#include "utils/ConfigManager.h"

LoginWidget::LoginWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
}
void LoginWidget::setupUi() {
    // ── Background — fills the entire widget ──────────────────────────────────
    m_bgLabel = new QLabel(this);
    m_bgLabel->setObjectName("LoginBg");
    m_bgLabel->setScaledContents(true);
    m_bgLabel->setPixmap(QPixmap(":/login_bg.png"));
    // No layout — positioned manually in resizeEvent

    // ── Form panel — floats over the background, anchored to the right ────────
    m_formPanel = new QWidget(this);   // parent is 'this', not in any layout
    m_formPanel->setObjectName("LoginFormPanel");
    m_formPanel->setFixedWidth(360);

    auto* vl = new QVBoxLayout(m_formPanel);
    vl->setContentsMargins(40, 0, 40, 0);
    vl->setSpacing(14);
    vl->setAlignment(Qt::AlignCenter);

    // Title — larger, bold
    m_titleLabel = new QLabel("账号登录", m_formPanel);
    m_titleLabel->setObjectName("LoginTitle");
    m_titleLabel->setAlignment(Qt::AlignLeft);

    // Subtitle — muted hint below the title
    auto* subtitleLabel = new QLabel("请登录您的账号以继续", m_formPanel);
    subtitleLabel->setObjectName("LoginSubtitle");
    subtitleLabel->setAlignment(Qt::AlignLeft);

    // Username
    m_userEdit = new QLineEdit(m_formPanel);
    m_userEdit->setPlaceholderText("账号");
    m_userEdit->setObjectName("LoginField");
    m_userEdit->setFixedHeight(42);
    m_userEdit->setClearButtonEnabled(true);

    // Password
    m_passEdit = new QLineEdit(m_formPanel);
    m_passEdit->setPlaceholderText("密码");
    m_passEdit->setObjectName("LoginField");
    m_passEdit->setFixedHeight(42);
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_passEdit->setClearButtonEnabled(true);

    // ── Captcha Row (图片 + 刷新按钮) ──

    // ==================== 验证码区域（新布局：输入框在左，图片在右） ====================
    auto* captchaRow = new QHBoxLayout();
    captchaRow->setContentsMargins(0, 0, 0, 0);
    captchaRow->setSpacing(12);

    // 验证码输入框（左边）
    m_captchaEdit = new QLineEdit(m_formPanel);
    m_captchaEdit->setPlaceholderText("验证码");
    m_captchaEdit->setObjectName("LoginField");
    m_captchaEdit->setFixedHeight(42);
    m_captchaEdit->setClearButtonEnabled(true);
    m_captchaEdit->setMinimumWidth(100);

    // 可点击的验证码图片（右边）
    m_captchaImageLabel = new ClickableLabel(m_formPanel);
    m_captchaImageLabel->setObjectName("CaptchaImageLabel");
    m_captchaImageLabel->setFixedSize(130, 42);
    m_captchaImageLabel->setScaledContents(true);
    m_captchaImageLabel->setAlignment(Qt::AlignCenter);
    m_captchaImageLabel->setStyleSheet("border: 1px solid #d0d0d0; background: #fafafa;");

    captchaRow->addWidget(m_captchaEdit);
    captchaRow->addWidget(m_captchaImageLabel);

    // Error label
    m_errorLabel = new QLabel(m_formPanel);
    m_errorLabel->setObjectName("ErrorLabel");
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setVisible(false);

    // Login button
    m_loginBtn = new QPushButton("登 录", m_formPanel);
    m_loginBtn->setObjectName("PrimaryButton");
    m_loginBtn->setFixedHeight(42);
    m_loginBtn->setEnabled(false);

    // Options row
    auto* optRow = new QHBoxLayout();
    optRow->setContentsMargins(0, 0, 0, 0);
    m_autoLoginChk = new QCheckBox("自动登录", m_formPanel);
    m_forgotLabel  = new QLabel(
        R"(<a href="#" style="color:#2468F2;text-decoration:none;">忘记密码</a>)",
        m_formPanel);
    m_forgotLabel->setOpenExternalLinks(false);
    m_forgotLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    optRow->addWidget(m_autoLoginChk);
    optRow->addStretch();
    optRow->addWidget(m_forgotLabel);

    // Spinner
    m_spinner = new QProgressBar(m_formPanel);
    m_spinner->setObjectName("LoginSpinner");
    m_spinner->setFixedHeight(3);
    m_spinner->setRange(0, 0);
    m_spinner->setTextVisible(false);
    m_spinner->setVisible(false);

    vl->addWidget(m_titleLabel);
    vl->addWidget(subtitleLabel);
    vl->addSpacing(16);
    vl->addWidget(m_userEdit);
    vl->addWidget(m_passEdit);
    vl->addLayout(captchaRow);


    vl->addWidget(m_errorLabel);
    vl->addWidget(m_loginBtn);
    vl->addLayout(optRow);
    vl->addWidget(m_spinner);
}

void LoginWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);

    QString host = QString::fromStdString(
        ConfigManager::instance().get<std::string>("server.host").value_or("")
    );
    int port = ConfigManager::instance().get<int>("server.port", 0);
    //QString port = QString::fromStdString(ConfigManager::instance().get<std::string>("server.port").value_or(""));
    QString captchaImageUrl = host + ":" + QString::number(port);
    QString endpoint = QString::fromStdString(ConfigManager::instance().get<std::string>("server.captcha_endpoint").value_or("/revelation/captchaImage"));

    QString fullHost = host;
    if (port != 0) fullHost += ":" + QString::number(port);
    m_captchaBaseUrl = fullHost + endpoint;

    // use http get to request from endpoint to get captcha image and display it in the form
    // the returned json like {"msg":"操作成功","img":"base64code","code":200,"captchaEnabled":true,"uuid":"e6dd647df0244ce48d9cca9e62ac5c0f"}
    // the img field is the base64 code of the captcha image, help to modify code to decode it and display it in the form below the password field, and add a new line edit for the user to input the captcha code, and a refresh button to refresh the captcha image
    // the code should be pro-level, and you could capulate http request(get, post), json parsing, base64 decoding etc.
    // need proper error handling and user feedback (e.g. show error message if captcha fetch fails, disable login button until captcha is correctly entered, etc.)
    // 
    // 2. Perform your logic
    // It's a good habit to check if the window is actually being shown 
    // (not just a state change)
    if (!event->spontaneous()) {
        refreshCaptcha();
   //     NetworkManager::instance().fetchCaptcha(m_captchaBaseUrl, [this](bool success, const QString& imgBase64, const QString& uuid, QString errStr) {
   //         if (success) {
   //             m_captchaUuid = uuid;
   //             m_captchaEnabled = errStr.isEmpty();
   //             if (m_captchaEnabled) {
   //                 // Decode base64 and set pixmap
   //                 QByteArray imgData = QByteArray::fromBase64(imgBase64.toUtf8());
   //                 QPixmap pix;
   //                 pix.loadFromData(imgData);
   //                 m_captchaImageLabel->setPixmap(pix);
   //                 // Show captcha UI elements
   //                 m_captchaImageLabel->setVisible(true);
   //                 m_captchaEdit->setVisible(true);
   //             } else {
   //                 // Hide captcha UI elements if not needed
   //                 m_captchaImageLabel->setVisible(false);
   //                 m_captchaEdit->setVisible(false);
   //             }
   //         } else {
   //             showError("验证码加载失败，请检查网络连接。");
   //         }
			//});
    }
}

void LoginWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // Background always covers the full widget
    if (m_bgLabel)
        m_bgLabel->setGeometry(0, 0, width(), height());

    // Form panel docked to the right edge, full height
    if (m_formPanel)
        m_formPanel->setGeometry(
            width() - m_formPanel->width(), 0,
            m_formPanel->width(), height());
}

void LoginWidget::setupConnections() {
    connect(m_userEdit, &QLineEdit::textChanged,
            this, &LoginWidget::onFieldsChanged);
    connect(m_passEdit, &QLineEdit::textChanged,
            this, &LoginWidget::onFieldsChanged);
    connect(m_loginBtn, &QPushButton::clicked,
            this, &LoginWidget::onLoginClicked);

    // Allow Enter key in password field to submit
    connect(m_passEdit, &QLineEdit::returnPressed,
            this, &LoginWidget::onLoginClicked);
    connect(m_userEdit, &QLineEdit::returnPressed,
            m_passEdit, QOverload<>::of(&QWidget::setFocus));

    connect(m_captchaEdit, &QLineEdit::textChanged, this, &LoginWidget::onFieldsChanged);

    connect(m_captchaImageLabel, &ClickableLabel::clicked, this, &LoginWidget::refreshCaptcha);

    // 接收 NetworkManager 的验证码结果
    connect(&NetworkManager::instance(), &NetworkManager::captchaFetched,
        this, &LoginWidget::onCaptchaFetched);

    //connect(m_captchaImageLabel, &ClickableLabel::clicked,
    //    &NetworkManager::instance(), &NetworkManager::fetchCaptcha);
}

// ── Public API ────────────────────────────────────────────────────────────────

void LoginWidget::setLoading(bool on) {
    m_loginBtn->setEnabled(!on);
    m_userEdit->setEnabled(!on);
    m_passEdit->setEnabled(!on);
    if (m_captchaEdit) m_captchaEdit->setEnabled(!on);
    m_spinner->setVisible(on);
}

void LoginWidget::showError(const QString& msg) {
    m_errorLabel->setText(msg);
    m_errorLabel->setVisible(!msg.isEmpty());
}

void LoginWidget::clearFields() {
    m_userEdit->clear();
    m_passEdit->clear();
    if (m_captchaEdit) m_captchaEdit->clear();
    showError({});
    setLoading(false);
    m_userEdit->setFocus();
}

// ── Private slots ─────────────────────────────────────────────────────────────

void LoginWidget::onLoginClicked() {
    const QString user = m_userEdit->text().trimmed();
    const QString pass = m_passEdit->text();
    const QString captcha = m_captchaEdit ? m_captchaEdit->text().trimmed() : QString();

    if (user.isEmpty() || pass.isEmpty()) return;
    if (m_captchaEnabled && captcha.isEmpty()) {
        showError("请输入验证码");
        return;
    }

    showError({});
    emit loginRequested(user, pass, captcha, m_captchaUuid);
}

void LoginWidget::onFieldsChanged() {
    const bool filled = !m_userEdit->text().trimmed().isEmpty()
                     && !m_passEdit->text().isEmpty();
    m_loginBtn->setEnabled(filled);
}

void LoginWidget::refreshCaptcha()
{
    if (!m_captchaBaseUrl.isEmpty()) {
        NetworkManager::instance().fetchCaptcha(m_captchaBaseUrl);
    }
}

void LoginWidget::onCaptchaFetched(bool success,
    const QString& imgBase64,
    const QString& uuid,
    bool captchaEnabled,
    const QString& errorMsg)
{
    if (!success) {
        showError(errorMsg.isEmpty() ? "获取验证码失败" : errorMsg);
        return;
    }

    m_captchaUuid = uuid;
    m_captchaEnabled = captchaEnabled;

    if (!captchaEnabled || imgBase64.isEmpty()) {
        m_captchaImageLabel->setVisible(false);
        m_captchaEdit->setVisible(false);
        onFieldsChanged();
        return;
    }

    // 处理可能的 data:image/... 前缀
    QString cleanBase64 = imgBase64;
    if (cleanBase64.startsWith("data:image")) {
        int commaPos = cleanBase64.indexOf(',');
        if (commaPos != -1) cleanBase64 = cleanBase64.mid(commaPos + 1);
    }

    QByteArray imageData = QByteArray::fromBase64(cleanBase64.toUtf8());
    QPixmap pixmap;
    if (pixmap.loadFromData(imageData)) {
        m_captchaImageLabel->setPixmap(
            pixmap.scaled(m_captchaImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );

        m_captchaImageLabel->setVisible(true);
        m_captchaEdit->setVisible(true);
        m_captchaEdit->clear();

        showError({});
        onFieldsChanged();
    }
    else {
        showError("验证码图片解码失败");
    }
}

// void LoginWidget::setupUi()
// {
//     // No layout for the main widget itself – we will manually position children.
//     // The background label becomes the "canvas".
//     m_bgLabel = new QLabel(this);
//     m_bgLabel->setObjectName("LoginBg");
//     m_bgLabel->setScaledContents(true);
//     m_bgLabel->setPixmap(QPixmap(":/login_bg.png"));

//     // Form Panel (floating on top)
//     auto* rightPanel = new QWidget(this); // Parent is 'this', not placed in a layout
//     rightPanel->setObjectName("LoginFormPanel");
//     rightPanel->setFixedWidth(360);
//     rightPanel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

//     // --- Inner layout of the form (remains exactly the same) ---
//     auto* vl = new QVBoxLayout(rightPanel);
//     vl->setContentsMargins(40, 0, 40, 0);
//     vl->setSpacing(12);
//     vl->setAlignment(Qt::AlignCenter);

//     // Title
//     m_titleLabel = new QLabel("账号登录", rightPanel);
//     m_titleLabel->setObjectName("LoginTitle");
//     m_titleLabel->setAlignment(Qt::AlignLeft);

//     // Username
//     m_userEdit = new QLineEdit(rightPanel);
//     m_userEdit->setPlaceholderText("请输入用户名");
//     m_userEdit->setObjectName("LoginField");
//     m_userEdit->setFixedHeight(40);
//     m_userEdit->setClearButtonEnabled(true);

//     // Password
//     m_passEdit = new QLineEdit(rightPanel);
//     m_passEdit->setPlaceholderText("请输入密码");
//     m_passEdit->setObjectName("LoginField");
//     m_passEdit->setFixedHeight(40);
//     m_passEdit->setEchoMode(QLineEdit::Password);
//     m_passEdit->setClearButtonEnabled(true);

//     // Error label
//     m_errorLabel = new QLabel(rightPanel);
//     m_errorLabel->setObjectName("ErrorLabel");
//     m_errorLabel->setWordWrap(true);
//     m_errorLabel->setVisible(false);

//     // Login button
//     m_loginBtn = new QPushButton("登录", rightPanel);
//     m_loginBtn->setObjectName("PrimaryButton");
//     m_loginBtn->setFixedHeight(40);
//     m_loginBtn->setEnabled(false);

//     // Options row
//     auto* optRow = new QHBoxLayout();
//     m_autoLoginChk = new QCheckBox("自动登录", rightPanel);
//     m_forgotLabel = new QLabel(R"(<a href="#" style="color:#2468F2;text-decoration:none;">忘记密码</a>)", rightPanel);
//     m_forgotLabel->setOpenExternalLinks(false);
//     m_forgotLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
//     optRow->addWidget(m_autoLoginChk);
//     optRow->addStretch();
//     optRow->addWidget(m_forgotLabel);

//     // Spinner
//     m_spinner = new QProgressBar(rightPanel);
//     m_spinner->setObjectName("LoginSpinner");
//     m_spinner->setFixedHeight(3);
//     m_spinner->setRange(0, 0);
//     m_spinner->setTextVisible(false);
//     m_spinner->setVisible(false);

//     vl->addWidget(m_titleLabel);
//     vl->addSpacing(24);
//     vl->addWidget(m_userEdit);
//     vl->addWidget(m_passEdit);
//     vl->addWidget(m_errorLabel);
//     vl->addWidget(m_loginBtn);
//     vl->addLayout(optRow);
//     vl->addWidget(m_spinner);
//     // --- End of inner layout ---

//     // Since we removed the QHBoxLayout, we must override resizeEvent
//     // to keep the background stretched and the panel positioned right.
// }

// void LoginWidget::resizeEvent(QResizeEvent *event)
// {
//     QWidget::resizeEvent(event);

//     // 1. Make background cover entire window
//     if (m_bgLabel) {
//         m_bgLabel->setGeometry(0, 0, width(), height());
//     }

//     // 2. Position form panel on the right, full height
//     if (auto* panel = findChild<QWidget*>("LoginFormPanel")) {
//         int panelWidth = panel->width();
//         panel->setGeometry(width() - panelWidth, 0, panelWidth, height());
//     }
// }


// void LoginWidget::setupUi() {
//     auto* hLayout = new QHBoxLayout(this);
//     hLayout->setContentsMargins(0, 0, 0, 0);
//     hLayout->setSpacing(0);

//     // ── Left: background image panel ──────────────────────────────────────────
//     m_bgLabel = new QLabel(this);
//     m_bgLabel->setObjectName("LoginBg");
//     m_bgLabel->setScaledContents(true);
//     m_bgLabel->setPixmap(QPixmap(":/login_bg.png"));
//     m_bgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
//     hLayout->addWidget(m_bgLabel, 3);   // ratio 3 : 2

//     // ── Right: form panel ─────────────────────────────────────────────────────
//     auto* rightPanel = new QWidget(this);
//     rightPanel->setObjectName("LoginFormPanel");
//     rightPanel->setFixedWidth(360);

//     auto* vl = new QVBoxLayout(rightPanel);
//     vl->setContentsMargins(40, 0, 40, 0);
//     vl->setSpacing(12);
//     vl->setAlignment(Qt::AlignCenter);

//     // Title
//     m_titleLabel = new QLabel("账号登录", rightPanel);
//     m_titleLabel->setObjectName("LoginTitle");
//     m_titleLabel->setAlignment(Qt::AlignLeft);

//     // Username
//     m_userEdit = new QLineEdit(rightPanel);
//     m_userEdit->setPlaceholderText("请输入用户名");
//     m_userEdit->setObjectName("LoginField");
//     m_userEdit->setFixedHeight(40);
//     m_userEdit->setClearButtonEnabled(true);

//     // Password
//     m_passEdit = new QLineEdit(rightPanel);
//     m_passEdit->setPlaceholderText("请输入密码");
//     m_passEdit->setObjectName("LoginField");
//     m_passEdit->setFixedHeight(40);
//     m_passEdit->setEchoMode(QLineEdit::Password);
//     m_passEdit->setClearButtonEnabled(true);

//     // Error label (hidden until needed)
//     m_errorLabel = new QLabel(rightPanel);
//     m_errorLabel->setObjectName("ErrorLabel");
//     m_errorLabel->setWordWrap(true);
//     m_errorLabel->setVisible(false);

//     // Login button
//     m_loginBtn = new QPushButton("登录", rightPanel);
//     m_loginBtn->setObjectName("PrimaryButton");
//     m_loginBtn->setFixedHeight(40);
//     m_loginBtn->setEnabled(false);      // disabled until both fields have text

//     // Options row: auto-login + forgot password
//     auto* optRow = new QHBoxLayout();
//     m_autoLoginChk = new QCheckBox("自动登录", rightPanel);
//     m_forgotLabel  = new QLabel(
//         R"(<a href="#" style="color:#2468F2;text-decoration:none;">忘记密码</a>)",
//         rightPanel);
//     m_forgotLabel->setOpenExternalLinks(false);
//     m_forgotLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
//     optRow->addWidget(m_autoLoginChk);
//     optRow->addStretch();
//     optRow->addWidget(m_forgotLabel);

//     // Spinner (indeterminate progress bar, hidden by default)
//     m_spinner = new QProgressBar(rightPanel);
//     m_spinner->setObjectName("LoginSpinner");
//     m_spinner->setFixedHeight(3);
//     m_spinner->setRange(0, 0);          // indeterminate mode
//     m_spinner->setTextVisible(false);
//     m_spinner->setVisible(false);

//     vl->addWidget(m_titleLabel);
//     vl->addSpacing(24);
//     vl->addWidget(m_userEdit);
//     vl->addWidget(m_passEdit);
//     vl->addWidget(m_errorLabel);
//     vl->addWidget(m_loginBtn);
//     vl->addLayout(optRow);
//     vl->addWidget(m_spinner);

//     hLayout->addWidget(rightPanel, 2);
// }

