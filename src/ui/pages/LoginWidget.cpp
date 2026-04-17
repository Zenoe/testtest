#include "LoginWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeyEvent>

LoginWidget::LoginWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
}
void LoginWidget::setupUi()
{
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
    m_userEdit->setPlaceholderText("请输入用户名");
    m_userEdit->setObjectName("LoginField");
    m_userEdit->setFixedHeight(42);
    m_userEdit->setClearButtonEnabled(true);

    // Password
    m_passEdit = new QLineEdit(m_formPanel);
    m_passEdit->setPlaceholderText("请输入密码");
    m_passEdit->setObjectName("LoginField");
    m_passEdit->setFixedHeight(42);
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_passEdit->setClearButtonEnabled(true);

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
    vl->addWidget(m_errorLabel);
    vl->addWidget(m_loginBtn);
    vl->addLayout(optRow);
    vl->addWidget(m_spinner);
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
// ── Setup ─────────────────────────────────────────────────────────────────────
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
}

// ── Public API ────────────────────────────────────────────────────────────────

void LoginWidget::setLoading(bool on) {
    m_loginBtn->setEnabled(!on);
    m_userEdit->setEnabled(!on);
    m_passEdit->setEnabled(!on);
    m_spinner->setVisible(on);
}

void LoginWidget::showError(const QString& msg) {
    m_errorLabel->setText(msg);
    m_errorLabel->setVisible(!msg.isEmpty());
}

void LoginWidget::clearFields() {
    m_userEdit->clear();
    m_passEdit->clear();
    showError({});
    setLoading(false);
    m_userEdit->setFocus();
}

// ── Private slots ─────────────────────────────────────────────────────────────

void LoginWidget::onLoginClicked() {
    const QString user = m_userEdit->text().trimmed();
    const QString pass = m_passEdit->text();

    if (user.isEmpty() || pass.isEmpty()) return;

    showError({});      // clear any previous error
    emit loginRequested(user, pass);
}

void LoginWidget::onFieldsChanged() {
    const bool filled = !m_userEdit->text().trimmed().isEmpty()
                     && !m_passEdit->text().isEmpty();
    m_loginBtn->setEnabled(filled);
}
