// ui/pages/ServerConfigWidget.cpp
#include "ServerConfigWidget.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QIntValidator>
#include <QRegularExpressionValidator>

ServerConfigWidget::ServerConfigWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
}
void ServerConfigWidget::setupUi()
{
    // ── Background ────────────────────────────────────────────────────────────
    m_bgLabel = new QLabel(this);
    m_bgLabel->setObjectName("LoginBg");        // reuse same QSS rule
    m_bgLabel->setScaledContents(true);
    m_bgLabel->setPixmap(QPixmap(":/login_bg.png"));

    // ── Form panel — floats over background, anchored right ──────────────────
    m_formPanel = new QWidget(this);
    m_formPanel->setObjectName("LoginFormPanel"); // reuse same QSS rule
    m_formPanel->setFixedWidth(360);

    auto* vl = new QVBoxLayout(m_formPanel);
    vl->setContentsMargins(40, 0, 40, 0);
    vl->setSpacing(14);
    vl->setAlignment(Qt::AlignCenter);

    // Title
    auto* titleLabel = new QLabel("新增服务器信息", m_formPanel);
    titleLabel->setObjectName("LoginTitle");     // reuse bold title style
    titleLabel->setAlignment(Qt::AlignLeft);

    // Subtitle
    auto* subtitleLabel = new QLabel("请填写服务器连接信息", m_formPanel);
    subtitleLabel->setObjectName("LoginSubtitle");
    subtitleLabel->setAlignment(Qt::AlignLeft);

    // Host field
    m_hostEdit = new QLineEdit(m_formPanel);
    m_hostEdit->setPlaceholderText("请输入服务器域名或IP");
    m_hostEdit->setObjectName("LoginField");
    m_hostEdit->setFixedHeight(42);
    m_hostEdit->setClearButtonEnabled(true);

    // Validation hint for host
    auto* hostHint = new QLabel("请输入服务器域名或IP", m_formPanel);
    hostHint->setObjectName("FieldHint");
    hostHint->setVisible(false);
    connect(m_hostEdit, &QLineEdit::textChanged,
            this, [hostHint](const QString& t) {
                hostHint->setVisible(t.trimmed().isEmpty());
            });

    // Port field
    m_portEdit = new QLineEdit(m_formPanel);
    m_portEdit->setPlaceholderText("请输入端口号");
    m_portEdit->setObjectName("LoginField");
    m_portEdit->setFixedHeight(42);
    m_portEdit->setValidator(new QIntValidator(1, 65535, m_portEdit));

    // Validation hint for port
    auto* portHint = new QLabel("请输入端口号", m_formPanel);
    portHint->setObjectName("FieldHint");
    portHint->setVisible(false);
    connect(m_portEdit, &QLineEdit::textChanged,
            this, [portHint](const QString& t) {
                portHint->setVisible(t.trimmed().isEmpty());
            });

    // Group combo (optional)
    m_groupCombo = new QComboBox(m_formPanel);
    m_groupCombo->setEditable(true);
    m_groupCombo->setFixedHeight(42);
    m_groupCombo->lineEdit()->setPlaceholderText(
        "请选择或者添加分组 (非必填)");

    // Status label
    m_statusLabel = new QLabel(m_formPanel);
    m_statusLabel->setObjectName("StatusLabel");
    m_statusLabel->setVisible(false);

    // Button row
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(12);
    m_testBtn = new QPushButton("连通性测试", m_formPanel);
    m_testBtn->setObjectName("SecondaryButton");
    m_testBtn->setFixedHeight(42);
    m_testBtn->setEnabled(false);

    m_okBtn = new QPushButton("确 定", m_formPanel);
    m_okBtn->setObjectName("PrimaryButton");
    m_okBtn->setFixedHeight(42);
    m_okBtn->setEnabled(false);

    btnRow->addWidget(m_testBtn);
    btnRow->addWidget(m_okBtn);

    vl->addWidget(titleLabel);
    vl->addWidget(subtitleLabel);
    vl->addSpacing(16);
    vl->addWidget(m_hostEdit);
    vl->addWidget(hostHint);
    vl->addWidget(m_portEdit);
    vl->addWidget(portHint);
    vl->addWidget(m_groupCombo);
    vl->addWidget(m_statusLabel);
    vl->addLayout(btnRow);

    // Connections
    connect(m_hostEdit, &QLineEdit::textChanged,
            this, &ServerConfigWidget::validateInputs);
    connect(m_portEdit, &QLineEdit::textChanged,
            this, &ServerConfigWidget::validateInputs);
    connect(m_testBtn, &QPushButton::clicked,
            this, &ServerConfigWidget::onTestClicked);
    connect(m_okBtn,   &QPushButton::clicked,
            this, &ServerConfigWidget::onOkClicked);
}

void ServerConfigWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    if (m_bgLabel)
        m_bgLabel->setGeometry(0, 0, width(), height());

    if (m_formPanel)
        m_formPanel->setGeometry(
            width() - m_formPanel->width(), 0,
            m_formPanel->width(), height());
}

void ServerConfigWidget::validateInputs() {
    bool valid = !m_hostEdit->text().trimmed().isEmpty()
              && !m_portEdit->text().isEmpty();
    m_okBtn->setEnabled(valid);
    m_testBtn->setEnabled(valid);
}

ServerConfig ServerConfigWidget::currentConfig() const {
    return { m_hostEdit->text().trimmed(),
             (quint16)m_portEdit->text().toInt(),
             m_groupCombo->currentText().trimmed() };
}

void ServerConfigWidget::onOkClicked() {
    emit configSaved(currentConfig());
}

void ServerConfigWidget::onTestClicked() {
    m_statusLabel->setText("Testing…");
    m_statusLabel->setVisible(true);
    emit testRequested();
}

void ServerConfigWidget::onTestResult(bool ok, const QString& msg) {
    m_statusLabel->setText(ok ? "✓ " + msg : "✗ " + msg);
    m_statusLabel->setProperty("ok", ok);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
    m_statusLabel->setVisible(true);
}
