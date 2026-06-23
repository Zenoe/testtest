#include "RingLogDialog.h"

#include "backend/ControlServiceClient.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QThreadPool>
#include <QVBoxLayout>

namespace {

constexpr char kStyle[] = R"(
QDialog#ringLogDialog {
    background: #F5F7FA;
}
QLabel#logTitle {
    color: #172033;
    font-size: 18px;
    font-weight: 700;
}
QLabel#logSubtitle, QLabel#logStatus {
    color: #7A8496;
    font-size: 11px;
}
QPlainTextEdit#ringLogView {
    background: #111827;
    color: #D1E7DD;
    border: 1px solid #283548;
    border-radius: 10px;
    padding: 10px;
    selection-background-color: #315CA8;
    font-family: "Cascadia Mono", "Consolas", monospace;
    font-size: 11px;
}
QPushButton#logAction {
    min-width: 76px;
    min-height: 32px;
    max-height: 32px;
    padding: 0 12px;
}
)";

} // namespace

RingLogDialog::RingLogDialog(const QString& configPath, QWidget* parent)
    : QDialog(parent),
      m_configPath(configPath)
{
    setObjectName("ringLogDialog");
    setWindowTitle("VPN diagnostic log");
    setAttribute(Qt::WA_DeleteOnClose);
    resize(820, 520);
    setMinimumSize(620, 380);
    setStyleSheet(kStyle);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 16);
    root->setSpacing(12);

    auto* title = new QLabel("VPN diagnostic log", this);
    title->setObjectName("logTitle");
    auto* subtitle = new QLabel("Latest entries from log.bin beside the tunnel config", this);
    subtitle->setObjectName("logSubtitle");
    root->addWidget(title);
    root->addWidget(subtitle);

    m_logView = new QPlainTextEdit(this);
    m_logView->setObjectName("ringLogView");
    m_logView->setReadOnly(true);
    m_logView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_logView->setPlaceholderText("No ring log entries are available.");
    root->addWidget(m_logView, 1);

    auto* actions = new QHBoxLayout;
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("logStatus");
    m_refreshButton = new QPushButton("Refresh", this);
    m_refreshButton->setObjectName("logAction");
    auto* copyButton = new QPushButton("Copy all", this);
    copyButton->setObjectName("logAction");
    auto* closeButton = new QPushButton("Close", this);
    closeButton->setObjectName("logAction");

    actions->addWidget(m_statusLabel);
    actions->addStretch();
    actions->addWidget(m_refreshButton);
    actions->addWidget(copyButton);
    actions->addWidget(closeButton);
    root->addLayout(actions);

    connect(m_refreshButton, &QPushButton::clicked, this, &RingLogDialog::refresh);
    connect(copyButton, &QPushButton::clicked, this, &RingLogDialog::copyAll);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);

    refresh();
}

void RingLogDialog::refresh() {
    m_refreshButton->setEnabled(false);
    m_statusLabel->setText("Loading...");
    const QPointer<RingLogDialog> self(this);
    const QString configPath = m_configPath;
    QThreadPool::globalInstance()->start([self, configPath] {
        const RingLogResponse response = ControlServiceClient::queryRingLog(configPath);
        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self, response] {
            if (self)
                self->applyResult(response.ok, response.lines, response.detail);
        }, Qt::QueuedConnection);
    });
}

void RingLogDialog::copyAll() {
    QApplication::clipboard()->setText(m_logView->toPlainText());
    m_statusLabel->setText("Copied to clipboard");
}

void RingLogDialog::applyResult(bool ok, const QStringList& lines, const QString& detail) {
    m_refreshButton->setEnabled(true);
    if (!ok) {
        m_statusLabel->setText(detail.isEmpty() ? "Unable to load log" : detail);
        return;
    }

    m_logView->setPlainText(lines.join('\n'));
    m_logView->verticalScrollBar()->setValue(m_logView->verticalScrollBar()->maximum());
    m_statusLabel->setText(QStringLiteral("%1 entries").arg(lines.size()));
}
