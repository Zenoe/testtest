#include "LoginWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QTimer>
#include "backend/NetworkManager.h"
#include "utils/ConfigManager.h"
#include "backend/SessionManager.h"
#include "secure/SecureStorageFactory.h"
#include "system/TerminalInfoProvider.h"
#include "ui/components/TerminalRegistrationDialog.h"
#include "utils/logger.h"

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
    m_userEdit->setText("lzy");

    // Password
    m_passEdit = new QLineEdit(m_formPanel);
    m_passEdit->setPlaceholderText("密码");
    m_passEdit->setObjectName("LoginField");
    m_passEdit->setFixedHeight(42);
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_passEdit->setClearButtonEnabled(true);
	m_passEdit->setText("1qaz@WSX"); // for testing, should be removed in production

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
    m_msgLabel = new QLabel(m_formPanel);
    m_msgLabel->setObjectName("msgLabel");
    m_msgLabel->setWordWrap(true);
	//m_msgLabel->setVisible(false);
    //m_msgLabel->setFixedHeight(20);

    // Login button
    m_loginBtn = new QPushButton("登 录", m_formPanel);
    m_loginBtn->setObjectName("PrimaryButton");
    m_loginBtn->setFixedHeight(42);
    m_loginBtn->setEnabled(false);

    m_loginBtn->installEventFilter(this);

    // Options row
    auto* optRow = new QHBoxLayout();
    optRow->setContentsMargins(0, 0, 0, 0);
    m_rememberMe = new QCheckBox("记住我", m_formPanel);
    m_forgotLabel  = new QLabel(
        R"(<a href="#" style="color:#2468F2;text-decoration:none;">忘记密码</a>)",
        m_formPanel);
    m_forgotLabel->setOpenExternalLinks(false);
    m_forgotLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    optRow->addWidget(m_rememberMe);
    optRow->addStretch();
    optRow->addWidget(m_forgotLabel);

    m_spinner = new QProgressBar(m_loginBtn); 
	// Set up the spinner as a floating bar at the bottom of the login button
    m_spinner->setObjectName("LoginSpinner");
    m_spinner->setFixedHeight(3);
    m_spinner->setRange(0, 0);
    m_spinner->setTextVisible(false);
    m_spinner->setStyleSheet(R"(
QProgressBar {
    border: none;
    background: transparent;
}
QProgressBar::chunk {
    background-color: #ffffff;   // 白色 loading 条
}
)");
    m_spinner->hide();

    vl->addWidget(m_titleLabel);
    vl->addWidget(subtitleLabel);
    vl->addSpacing(16);
    vl->addWidget(m_userEdit);
    vl->addWidget(m_passEdit);
    vl->addLayout(captchaRow);


    vl->addWidget(m_loginBtn);
    vl->addLayout(optRow);
    vl->addWidget(m_msgLabel);
}

void LoginWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);

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
      // 初始化加载动画计时器
      m_loadingTimer = new QTimer(this);
      connect(m_loadingTimer, &QTimer::timeout, this, &LoginWidget::updateLoadingDots);

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
      //             showMsg("验证码加载失败，请检查网络连接。", true);
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

	//qDebug() << "LoginBtn resized: " << m_loginBtn->width() << "x" << m_loginBtn->height();
    // can not catch the final size of the login button here
	//if (m_spinner && m_loginBtn) {
	//	int h = 3;
	//	m_spinner->setGeometry(
	//		0,
	//		m_loginBtn->height() - h,
	//		m_loginBtn->width(),
	//		h
	//	);
	//}
}

void LoginWidget::setupConnections() {
	connect(m_userEdit, &QLineEdit::textChanged, this, &LoginWidget::onFieldsChanged);
	connect(m_passEdit, &QLineEdit::textChanged, this, &LoginWidget::onFieldsChanged);
	connect(m_loginBtn, &QPushButton::clicked, this, &LoginWidget::onLoginClicked);

	// Allow Enter key in password field to submit
	connect(m_passEdit, &QLineEdit::returnPressed, this, &LoginWidget::onLoginClicked);
	connect(m_userEdit, &QLineEdit::returnPressed, m_passEdit, QOverload<>::of(&QWidget::setFocus));

	connect(m_captchaEdit, &QLineEdit::textChanged, this, &LoginWidget::onFieldsChanged);

	//connect(m_captchaImageLabel, &ClickableLabel::clicked, this, &LoginWidget::refreshCaptcha);
	connect(m_captchaImageLabel, &ClickableLabel::clicked,
		this, [this]() {
			m_preserveMessageOnCaptchaRefresh = false;
			this->showMsg({});
			this->refreshCaptcha();
		});

	// 接收 NetworkManager 的验证码结果
	connect(&NetworkManager::instance(), &NetworkManager::captchaFetched, this, &LoginWidget::onCaptchaFetched);
    connect(&NetworkManager::instance(), &NetworkManager::loginFinished, this, &LoginWidget::onLoginFinished);
    connect(&NetworkManager::instance(), &NetworkManager::terminalRegistrationChecked,
        this, &LoginWidget::onTerminalRegistrationChecked);
    connect(&NetworkManager::instance(), &NetworkManager::terminalRegistered,
        this, &LoginWidget::onTerminalRegistered);
}

// ── Public API ────────────────────────────────────────────────────────────────

void LoginWidget::setLoading(bool on) {
    m_loginBtn->setEnabled(!on);
    m_userEdit->setEnabled(!on);
    m_passEdit->setEnabled(!on);
    if (m_captchaEdit) m_captchaEdit->setEnabled(!on);
    if (on) {
        m_loginBtn->setText("");
        m_spinner->show();
    }
    else {
        m_loginBtn->setText("登 录");
        m_spinner->hide();
    }
}

void LoginWidget::showMsg(const QString& msg, bool isError) {
    m_msgLabel->setText(msg);
    if(isError) {
        m_msgLabel->setStyleSheet("color: #D32F2F;"); // 红色
		setLoading(false);
    } else {
        m_msgLabel->setStyleSheet("color: #388E3C;"); // 绿色
	}
    // m_msgLabel->setVisible(!msg.isEmpty());
}

void LoginWidget::clearFields() {
    m_preserveMessageOnCaptchaRefresh = false;
    m_userEdit->clear();
    m_passEdit->clear();
    if (m_captchaEdit) m_captchaEdit->clear();
    showMsg({});
    setLoading(false);
    m_userEdit->setFocus();
    refreshCaptcha();
}

// ── Private slots ─────────────────────────────────────────────────────────────
void LoginWidget::onLoginClicked()
{
    const QString user = m_userEdit->text().trimmed();
    const QString pass = m_passEdit->text();
    const QString captcha = m_captchaEdit ? m_captchaEdit->text().trimmed() : QString();

    if (user.isEmpty() || pass.isEmpty()) return;

    if (m_captchaEnabled && captcha.isEmpty()) {
        showMsg("请输入验证码", true);
        return;
    }

    setLoading(true);
	showMsg("正在登录...");

    NetworkManager::instance().login(
        user,
        pass,
        captcha,
        m_captchaUuid
    );
}

void LoginWidget::onLoginFinished(bool success, const QString& token, const QString& errorMsg) {
    // don't stop loading immediately, wait for vpn connection
    //setLoading(false);

    if (!success) {
        showMsg(errorMsg.isEmpty() ? "登录失败" : errorMsg, true);
        m_preserveMessageOnCaptchaRefresh = true;
        refreshCaptcha();
        return;
    }

    m_pendingToken = token;
    m_pendingUsername = m_userEdit->text().trimmed();
    m_pendingRememberMe = m_rememberMe->isChecked();
    m_postLoginPending = true;
    m_preserveMessageOnCaptchaRefresh = false;

    // Establish a non-persistent session so authenticated post-login checks can
    // attach the bearer token. Persistence only happens after registration.
    SessionManager::instance().setSession({ m_pendingUsername, token, false });

    showMsg(tr("登录成功，正在检查终端注册状态..."));
    const TerminalInfoSnapshot terminalInfo = TerminalInfoProvider::collect();
    m_terminalHardwareCode = terminalInfo.hardwareCode;
    if (m_terminalHardwareCode.isEmpty()) {
        abortPostLogin(tr("无法获取终端硬件特征码"));
        return;
    }

    NetworkManager::instance().checkTerminalRegistration(terminalInfo.payload);
}

void LoginWidget::onTerminalRegistrationChecked(bool success,
                                                bool registered,
                                                const QString& errorMsg)
{
    if (!m_postLoginPending)
        return;

    if (!success) {
        abortPostLogin(errorMsg.isEmpty() ? tr("终端注册状态检查失败") : errorMsg);
        return;
    }

    if (registered) {
        spdlog::info("LoginWidget: terminal already registered, continuing login");
        completePostLogin();
        return;
    }

    spdlog::info("LoginWidget: terminal registration required");
    showMsg(tr("终端尚未注册，请输入注册验证码"));
    showTerminalRegistrationDialog();
}

void LoginWidget::onTerminalRegistered(bool success,
                                       const QString& uninstallCode,
                                       const QString& errorMsg)
{
    if (!m_postLoginPending)
        return;

    if (!success) {
        if (m_registrationDialog) {
            m_registrationDialog->setSubmitting(false);
            m_registrationDialog->showError(
                errorMsg.isEmpty() ? tr("终端注册失败") : errorMsg);
        } else {
            abortPostLogin(errorMsg.isEmpty() ? tr("终端注册失败") : errorMsg);
        }
        return;
    }

    auto storage = SecureStorageFactory::create();
    QString storageError;
    const QString uninstallCodeKey =
        qApp->applicationDisplayName() + QStringLiteral("/terminal-uninstall-code");
    if (!storage->save(uninstallCodeKey, m_pendingUsername, uninstallCode, storageError)) {
        spdlog::error("LoginWidget: failed to securely store terminal uninstall code: {}",
            storageError.toStdString());
    }

    if (m_registrationDialog)
        m_registrationDialog->accept();

    showMsg(tr("终端注册成功，正在继续登录..."));
    completePostLogin();
}

void LoginWidget::showTerminalRegistrationDialog()
{
    if (m_registrationDialog) {
        m_registrationDialog->raise();
        m_registrationDialog->activateWindow();
        return;
    }

    m_registrationDialog = new TerminalRegistrationDialog(window());
    m_registrationDialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(m_registrationDialog, &TerminalRegistrationDialog::registrationRequested,
        this, [this](const QString& code) {
            if (!m_postLoginPending)
                return;
            NetworkManager::instance().registerTerminal(m_terminalHardwareCode, code);
        });

    connect(m_registrationDialog, &QDialog::rejected, this, [this]() {
        if (m_postLoginPending)
            abortPostLogin(tr("终端未注册，登录已取消"));
    });

    m_registrationDialog->show();
}

void LoginWidget::completePostLogin()
{
    if (!m_postLoginPending)
        return;

    auto storage = SecureStorageFactory::create();
    QString error;
    if (m_pendingRememberMe) {
        storage->save(qApp->applicationDisplayName(), m_pendingUsername, m_pendingToken, error);
    } else {
        storage->remove(qApp->applicationDisplayName(), error);
    }
    if (!error.isEmpty()) {
        spdlog::warn("LoginWidget: credential persistence operation reported: {}",
            error.toStdString());
    }

    SessionManager::instance().setSession(
        { m_pendingUsername, m_pendingToken, m_pendingRememberMe });

    const QString token = m_pendingToken;
    m_postLoginPending = false;
    m_pendingToken.clear();
    m_pendingUsername.clear();
    m_terminalHardwareCode.clear();

    emit loginSuccess(token);
}

void LoginWidget::abortPostLogin(const QString& message)
{
    spdlog::error("LoginWidget: post-login terminal validation failed: {}",
        message.toStdString());

    m_postLoginPending = false;
    m_pendingToken.clear();
    m_pendingUsername.clear();
    m_terminalHardwareCode.clear();
    SessionManager::instance().clearSession();

    m_preserveMessageOnCaptchaRefresh = true;
    showMsg(message, true);
    refreshCaptcha();
}

void LoginWidget::onFieldsChanged() {
	const bool filled = !m_userEdit->text().trimmed().isEmpty()
		&& !m_passEdit->text().isEmpty()
		&& !m_captchaEdit->text().trimmed().isEmpty();
	m_loginBtn->setEnabled(filled);
}

void LoginWidget::onCaptchaFetched(bool success,
                                   const QString& imgBase64,
                                   const QString& uuid,
                                   bool captchaEnabled,
                                   const QString& errorMsg)
{
    stopCaptchaLoading();                     // 无论成功失败都停止加载动画

    if (!success) {
        m_preserveMessageOnCaptchaRefresh = false;
        showMsg(errorMsg.isEmpty() ? "获取验证码失败" : errorMsg, true);
        return;
    }

    m_captchaUuid    = uuid;
    m_captchaEnabled = captchaEnabled;

    if (!captchaEnabled || imgBase64.isEmpty()) {
        m_captchaImageLabel->setVisible(false);
        m_captchaEdit->setVisible(false);
        m_preserveMessageOnCaptchaRefresh = false;
        onFieldsChanged();
        return;
    }

    // 处理 data: 前缀
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

        if (!m_preserveMessageOnCaptchaRefresh)
            showMsg({});
        m_preserveMessageOnCaptchaRefresh = false;
        onFieldsChanged();
    } else {
        m_preserveMessageOnCaptchaRefresh = false;
        showMsg("验证码图片解码失败", true);
    }
}

void LoginWidget::refreshCaptcha()
{
    if (m_isFetchingCaptcha) return;

    startCaptchaLoading();
 //   QString captchaUrl = getServerHost() + QString::fromStdString(
 //       ConfigManager::instance().get<std::string>("server.captcha_endpoint")
 //       .value_or("/revelation/captchaImage")
	//);
    //NetworkManager::instance().fetchCaptcha(captchaUrl);
    NetworkManager::instance().fetchCaptcha();
}

void LoginWidget::startCaptchaLoading()
{
    m_isFetchingCaptcha = true;
    m_captchaImageLabel->setEnabled(false);
    m_captchaImageLabel->setCursor(Qt::WaitCursor);

    m_dotsCount = 0;
    m_loadingTimer->start(300);           // 每300ms更新一次点
    m_captchaImageLabel->setText("加载中");
    m_captchaImageLabel->setPixmap(QPixmap());   // 清空旧图片
}

void LoginWidget::stopCaptchaLoading()
{
    m_isFetchingCaptcha = false;
    m_loadingTimer->stop();
    m_captchaImageLabel->setEnabled(true);
    m_captchaImageLabel->setCursor(Qt::PointingHandCursor);
    m_captchaImageLabel->setText("");
}

void LoginWidget::updateLoadingDots()
{
    m_dotsCount = (m_dotsCount + 1) % 4;
    QString dots = QString(".").repeated(m_dotsCount);
    m_captchaImageLabel->setText("加载中" + dots);
}

bool LoginWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_loginBtn && event->type() == QEvent::Resize) {
        int h = 3;
        m_spinner->setGeometry(
            0,
            m_loginBtn->height() - h,
            m_loginBtn->width(),
            h
        );
    }
    return QWidget::eventFilter(obj, event);
}


// void LoginWidget::setupUi()
// {
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
//     ...
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


