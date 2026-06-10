#include "StatusPanel.h"
#include "backend/ControlServiceClient.h"
#include "RingLogDialog.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QScreen>
#include <QShowEvent>
#include <QDateTime>
#include <QStyle>

#include <spdlog/spdlog.h>

static const char* kStyle = R"(
QWidget#statusPanelRoot {
    background: transparent;
}
QWidget#statusContainer {
    background: #FFFFFF;
    border: 1px solid #DDE3EC;
    border-radius: 14px;
}
QLabel#heading {
    color: #172033;
    font-size: 17px;
    font-weight: 700;
}
QLabel#subtitle {
    color: #7A8496;
    font-size: 11px;
}
QLabel#statusBadge {
    border-radius: 10px;
    padding: 3px 9px;
    font-size: 11px;
    font-weight: 600;
}
QLabel#statusBadge[state="online"] {
    color: #137333;
    background: #E6F4EA;
}
QLabel#statusBadge[state="waiting"] {
    color: #8A5A00;
    background: #FFF4D6;
}
QLabel#statusBadge[state="offline"] {
    color: #B3261E;
    background: #FCE8E6;
}
QFrame#metricCard {
    background: #F7F9FC;
    border: 1px solid #E8ECF2;
    border-radius: 10px;
}
QLabel#metricKey, QLabel#detailKey {
    color: #7A8496;
    font-size: 10px;
    font-weight: 600;
}
QLabel#rxValue, QLabel#txValue {
    color: #172033;
    font-size: 20px;
    font-weight: 700;
}
QLabel#metricAccentRx {
    color: #18864B;
    font-size: 14px;
    font-weight: 700;
}
QLabel#metricAccentTx {
    color: #2468F2;
    font-size: 14px;
    font-weight: 700;
}
QLabel#handshakeValue {
    color: #27344D;
    font-size: 12px;
    font-weight: 600;
}
QFrame#hline {
    background: #E8ECF2;
    max-height: 1px;
    border: none;
}
QPushButton#closeBtn {
    min-width: 0;
    max-width: 28px;
    min-height: 28px;
    max-height: 28px;
    background: transparent;
    border: none;
    color: #7A8496;
    font-size: 16px;
    font-weight: 600;
    padding: 0;
    border-radius: 8px;
}
QPushButton#closeBtn:hover {
    color: #172033;
    background: #EEF2F7;
}
QPushButton#closeBtn:pressed {
    background: #E3E8F0;
}
QPushButton#logButton {
    min-width: 0;
    min-height: 32px;
    max-height: 32px;
    color: #2468F2;
    background: #F0F5FF;
    border: 1px solid #C9D9FA;
    border-radius: 8px;
    padding: 0 12px;
    font-size: 11px;
    font-weight: 600;
}
QPushButton#logButton:hover {
    color: #1848B8;
    background: #E4EDFF;
    border-color: #9CB9F5;
}
)";
StatusPanel::StatusPanel(const QString& adapterName, const QString& configPath, QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_adapterName(adapterName)
    , m_configPath(configPath)
{
    setObjectName("statusPanelRoot");
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setStyleSheet(kStyle);
    buildUi();
    setFixedSize(360, 315);
}

StatusPanel::~StatusPanel()
{
    stopPoller();
}

// ---- toggle -----------------------------------------------------------------
void StatusPanel::toggle()
{
    if (isVisible())
        hide();
    else
        show();
}

void StatusPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // parentWidget() is now guaranteed to be MainWindow (set correctly by AvatarWidget)
    QWidget* anchor = parentWidget();
    QRect anchorRect;

    if (anchor) {
        QScreen* screen = anchor->screen(); // Qt 5.14+  retrieves the QScreen the window is actually on
        if (!screen) screen = QGuiApplication::primaryScreen();

        // Clamp to the screen the window actually lives on — prevents cross-monitor bleed
        anchorRect = anchor->frameGeometry().intersected(screen->availableGeometry());

        if (anchorRect.isEmpty()) {
            spdlog::warn("StatusPanel: anchor rect empty after screen clamp, using screen");
            anchorRect = screen->availableGeometry();
        }
    }
    else {
        spdlog::warn("StatusPanel: no parent, centering on primary screen");
        anchorRect = QGuiApplication::primaryScreen()->availableGeometry();
    }

    move(anchorRect.x() + (anchorRect.width() - width()) / 2,
        anchorRect.y() + (anchorRect.height() - height()) / 2);

    spdlog::debug("StatusPanel: shown at ({},{}) anchored to ({},{} {}x{})",
        x(), y(),
        anchorRect.x(), anchorRect.y(), anchorRect.width(), anchorRect.height());

    startPoller();
}

void StatusPanel::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    stopPoller();
}

void StatusPanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
		// Qt::Tool windows don't receive keyboard events unless they have focus. need eventFilter if we want to close on Esc without focus, but this is good enough for now.
        spdlog::debug("StatusPanel: Esc pressed → hiding");
        hide();          // triggers hideEvent → stopPoller
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ---- buildUi ----------------------------------------------------------------
void StatusPanel::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 16);

    auto* container = new QWidget(this);
    container->setObjectName("statusContainer");
    outer->addWidget(container);

    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    // ── Title row ──────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(10);

        auto* titleColumn = new QVBoxLayout;
        titleColumn->setSpacing(2);

        auto* title = new QLabel("VPN status", container);
        title->setObjectName("heading");
        auto* subtitle = new QLabel(m_adapterName, container);
        subtitle->setObjectName("subtitle");
        subtitle->setTextInteractionFlags(Qt::TextSelectableByMouse);
        titleColumn->addWidget(title);
        titleColumn->addWidget(subtitle);

        m_statusLabel = new QLabel("Connecting", container);
        m_statusLabel->setObjectName("statusBadge");
        m_statusLabel->setProperty("state", "waiting");

        auto* closeBtn = new QPushButton(QString::fromUtf8("\xC3\x97"), container);
        closeBtn->setObjectName("closeBtn");
        closeBtn->setToolTip("Close");
        connect(closeBtn, &QPushButton::clicked, this, &StatusPanel::toggle);

        row->addLayout(titleColumn);
        row->addStretch();
        row->addWidget(m_statusLabel, 0, Qt::AlignTop);
        row->addWidget(closeBtn);
        root->addLayout(row);
    }

    {
        auto* row = new QHBoxLayout;
        auto* hint = new QLabel("Controller and tunnel diagnostics", container);
        hint->setObjectName("subtitle");
        auto* logButton = new QPushButton("View diagnostic log", container);
        logButton->setObjectName("logButton");
        connect(logButton, &QPushButton::clicked, this, &StatusPanel::showRingLog);
        row->addWidget(hint);
        row->addStretch();
        row->addWidget(logButton);
        root->addLayout(row);
    }

    // ── Divider ────────────────────────────────────────────────
    auto makeLine = [&]() {
        auto* line = new QFrame(container);
        line->setObjectName("hline");
        line->setFrameShape(QFrame::HLine);
        return line;
        };

    root->addWidget(makeLine());

    // ── Metrics ────────────────────────────────────────────────
    {
        auto* metricsRow = new QHBoxLayout;
        metricsRow->setSpacing(10);

        auto makeMetric = [container](const QString& arrow, const QString& title,
                                      const char* accentName, QLabel*& value) {
            auto* card = new QFrame(container);
            card->setObjectName("metricCard");
            auto* layout = new QVBoxLayout(card);
            layout->setContentsMargins(12, 10, 12, 10);
            layout->setSpacing(4);

            auto* headingRow = new QHBoxLayout;
            auto* accent = new QLabel(arrow, card);
            accent->setObjectName(accentName);
            auto* key = new QLabel(title, card);
            key->setObjectName("metricKey");
            headingRow->addWidget(accent);
            headingRow->addWidget(key);
            headingRow->addStretch();

            value = new QLabel("0 B", card);
            value->setObjectName(title == "RECEIVED" ? "rxValue" : "txValue");
            layout->addLayout(headingRow);
            layout->addWidget(value);
            return card;
        };

        metricsRow->addWidget(makeMetric(QString::fromUtf8("\xE2\x86\x93"), "RECEIVED",
                                         "metricAccentRx", m_rxValue), 1);
        metricsRow->addWidget(makeMetric(QString::fromUtf8("\xE2\x86\x91"), "SENT",
                                         "metricAccentTx", m_txValue), 1);
        root->addLayout(metricsRow);
    }

    root->addWidget(makeLine());

    {
        auto* row = new QHBoxLayout;
        auto* key = new QLabel("LAST HANDSHAKE", container);
        key->setObjectName("detailKey");
        m_handshakeValue = new QLabel("Waiting for peer", container);
        m_handshakeValue->setObjectName("handshakeValue");
        m_handshakeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(key);
        row->addStretch();
        row->addWidget(m_handshakeValue);
        root->addLayout(row);
    }

    // ── Shadow ─────────────────────────────────────────────────
    auto* shadow = new QGraphicsDropShadowEffect(container);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(23, 32, 51, 45));
    container->setGraphicsEffect(shadow);
}

// ---- poller management ------------------------------------------------------
void StatusPanel::startPoller()
{
    if (m_thread)
        return;   // already running

    m_thread = new QThread(this);
    m_poller = new StatusPoller(m_adapterName);   // no Qt parent — moved to thread
    m_poller->moveToThread(m_thread);

    // Poller → panel  (queued: crosses thread boundary automatically)
    connect(m_poller, &StatusPoller::statsReady,
        this, &StatusPanel::onStats,
        Qt::QueuedConnection);

    connect(m_poller, &StatusPoller::errorOccurred,
        this, &StatusPanel::onError,
        Qt::QueuedConnection);

    // Thread lifecycle
    connect(m_thread, &QThread::started,
        m_poller, &StatusPoller::start,
        Qt::QueuedConnection);

    connect(m_thread, &QThread::finished,
        m_poller, &QObject::deleteLater);

    m_thread->start();
    spdlog::debug("StatusPanel: poller thread started");
}

void StatusPanel::stopPoller()
{
    if (!m_thread)
        return;

    // Ask poller to stop its timer (queued into the worker thread's event loop)
    QMetaObject::invokeMethod(m_poller, "stop", Qt::QueuedConnection);

    m_thread->quit();
    if (!m_thread->wait(3000))
        m_thread->terminate();

    // m_poller is deleted via deleteLater connected to QThread::finished
    delete m_thread;
    m_thread = nullptr;
    m_poller = nullptr;
    spdlog::debug("StatusPanel: poller thread stopped");
}

// ---- slots ------------------------------------------------------------------
void StatusPanel::onStats(quint64 rx, quint64 tx, qint64 lastHandshakeMsec)
{
    m_rxValue->setText(formatBytes(rx));
    m_txValue->setText(formatBytes(tx));
    m_handshakeValue->setText(formatHandshake(lastHandshakeMsec));

    if (lastHandshakeMsec <= 0) {
        m_handshakeValue->setToolTip("No successful handshake has been recorded");
        setConnectionState("Waiting for peer", "waiting");
        return;
    }

    const QDateTime handshake = QDateTime::fromMSecsSinceEpoch(lastHandshakeMsec);
    m_handshakeValue->setToolTip(
        QStringLiteral("Last handshake: %1").arg(handshake.toString(Qt::TextDate)));
    const qint64 elapsedSeconds =
        qMax<qint64>(0, handshake.secsTo(QDateTime::currentDateTime()));
    setConnectionState(elapsedSeconds <= 180 ? "Connected" : "Handshake stale",
                       elapsedSeconds <= 180 ? "online" : "waiting");
}

void StatusPanel::onError(const QString& msg)
{
    m_rxValue->setText("—");
    m_txValue->setText("—");
    m_handshakeValue->setText("Unavailable");
    m_handshakeValue->setToolTip(msg);
    setConnectionState("Unavailable", "offline");
}

void StatusPanel::showRingLog()
{
    if (!m_ringLogDialog) {
        QWidget* owner = parentWidget() ? parentWidget() : this;
        m_ringLogDialog = new RingLogDialog(m_configPath, owner);
    }
    m_ringLogDialog->show();
    m_ringLogDialog->raise();
    m_ringLogDialog->activateWindow();
}

// ---- formatBytes ------------------------------------------------------------
QString StatusPanel::formatBytes(quint64 bytes)
{
    constexpr quint64 kKiB = 1024ULL;
    constexpr quint64 kMiB = 1024ULL * kKiB;
    constexpr quint64 kGiB = 1024ULL * kMiB;

    if (bytes >= kGiB)
        return QString::number(double(bytes) / kGiB, 'f', 2) + " GiB";
    if (bytes >= kMiB)
        return QString::number(double(bytes) / kMiB, 'f', 2) + " MiB";
    if (bytes >= kKiB)
        return QString::number(double(bytes) / kKiB, 'f', 1) + " KiB";
    return QString::number(bytes) + " B";
}

QString StatusPanel::formatHandshake(qint64 lastHandshakeMsec)
{
    if (lastHandshakeMsec <= 0)
        return QStringLiteral("Never");

    const QDateTime handshake = QDateTime::fromMSecsSinceEpoch(lastHandshakeMsec);
    const qint64 elapsed = qMax<qint64>(0, handshake.secsTo(QDateTime::currentDateTime()));
    if (elapsed < 5)
        return QStringLiteral("Just now");
    if (elapsed < 60)
        return QStringLiteral("%1 sec ago").arg(elapsed);
    if (elapsed < 3600)
        return QStringLiteral("%1 min ago").arg(elapsed / 60);
    if (elapsed < 86400)
        return QStringLiteral("%1 hr ago").arg(elapsed / 3600);
    return QStringLiteral("%1 days ago").arg(elapsed / 86400);
}

void StatusPanel::setConnectionState(const QString& text, const QString& state)
{
    m_statusLabel->setText(text);
    m_statusLabel->setProperty("state", state);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}


// ============================================================
//  StatusPoller
// ============================================================
StatusPoller::StatusPoller(const QString& adapterName, QObject* parent)
    : QObject(parent)
    , m_adapterName(adapterName)
{
}

void StatusPoller::start()
{
    if (m_running)
        return;

    m_running = true;

    m_timer = new QTimer(this);          // parent = this → lives on worker thread
    m_timer->setInterval(1000);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout, this, &StatusPoller::poll);
    m_timer->start();

    // Fire once immediately so the UI isn't blank for the first second
    poll();

    spdlog::debug("StatusPoller: started on thread {:p}",
        static_cast<void*>(QThread::currentThread()));
}

void StatusPoller::stop()
{
    m_running = false;
    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
}
void StatusPoller::poll()
{
    if (!m_running)
        return;

    const TrafficStatsResponse response =
        ControlServiceClient::queryTraffic(m_adapterName, 2'000);
    if (response.ok) {
        if (m_consecutiveFailures > 0) {
            spdlog::info(
                "StatusPoller: traffic polling recovered | adapter={} failures={}",
                m_adapterName.toStdString(), m_consecutiveFailures);
        }
        m_consecutiveFailures = 0;
        m_lastError.clear();
        emit statsReady(response.rxBytes, response.txBytes, response.lastHandshakeMsec);
        return;
    }

    ++m_consecutiveFailures;
    const QString detail = response.detail.isEmpty()
        ? QStringLiteral("Adapter unavailable")
        : response.detail;
    if (m_consecutiveFailures == 1
        || m_consecutiveFailures % 30 == 0
        || detail != m_lastError) {
        spdlog::warn(
            "StatusPoller: traffic poll failed | adapter={} failures={} error={}",
            m_adapterName.toStdString(), m_consecutiveFailures, detail.toStdString());
    }
    m_lastError = detail;
    emit errorOccurred(detail);
}
