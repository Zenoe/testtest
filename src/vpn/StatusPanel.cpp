#include "StatusPanel.h"
#include "Driver.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QScreen>
#include <QShowEvent>

#include <spdlog/spdlog.h>
#include <stdexcept>

static const char* kStyle = R"(
QLabel#heading {
    color: rgba(180, 180, 220, 0.9);
    font: 500 10px "Segoe UI", "SF Pro Text", "Consolas";
    letter-spacing: 2px;
}
QLabel#metricKey {
    color: rgba(150, 150, 200, 0.9);
    font: 9px "Segoe UI", "SF Pro Text", "Consolas";
    letter-spacing: 1.5px;
}
QLabel#rxValue {
    color: #6ee847;
    font: 600 22px "Segoe UI", "SF Pro Display", "Consolas";
}
QLabel#txValue {
    color: #4d9fff;
    font: 600 22px "Segoe UI", "SF Pro Display", "Consolas";
}
QLabel#status {
    color: rgba(255, 100, 100, 0.85);
    font: 10px "Segoe UI", "SF Pro Text", "Consolas";
    letter-spacing: 0.5px;
}
QFrame#hline {
    background: qlineargradient(
        x1:0, y1:0, x2:1, y2:0,
        stop:0 transparent,
        stop:0.5 rgba(120, 120, 200, 0.25),
        stop:1 transparent
    );
    max-height: 1px;
    border: none;
}
QPushButton#closeBtn {
    background: rgba(255, 255, 255, 0.06);
    border: 1px solid rgba(255, 255, 255, 0.10);
    color: rgba(180, 180, 220, 0.6);
    font: 600 14px "Segoe UI", "SF Pro Text";
    padding: 0;
    border-radius: 6px;
}
QPushButton#closeBtn:hover {
    color: #ffffff;
    background: rgba(255, 255, 255, 0.12);
    border-color: rgba(255, 255, 255, 0.18);
}
QPushButton#closeBtn:pressed {
    background: rgba(255, 255, 255, 0.16);
}
)";
StatusPanel::StatusPanel(const QString& configFile, QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_configFile(configFile)
{
    //setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setStyleSheet(kStyle);
    buildUi();
    setFixedSize(280, 180);
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
    auto* container = new QWidget(this);
    container->setObjectName("statusContainer");
    container->setStyleSheet(R"(
    #statusContainer {
        background-color: #eeeeee;
        border-radius: 4px;
        border: 1px solid #DCDCDC;
    }
)");
    container->setFixedSize(280, 180);
    //root0->addWidget(container);

    auto* root = new QVBoxLayout(container);
    root->setSpacing(8);

    // ── Title row ──────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(0);

        auto* title = new QLabel("STATUS STATS", container);
        title->setObjectName("heading");

        auto* closeBtn = new QPushButton("✕", container);
        closeBtn->setObjectName("closeBtn");
        closeBtn->setFixedSize(24, 24);
        connect(closeBtn, &QPushButton::clicked, this, &StatusPanel::toggle);

        row->addWidget(title);
        row->addStretch();
        row->addWidget(closeBtn);
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
        metricsRow->setSpacing(24);

        // RX
        {
            auto* col = new QVBoxLayout;
            auto* key = new QLabel("RECEIVED", container);
            key->setObjectName("metricKey");

            m_rxValue = new QLabel("0", container);
            m_rxValue->setObjectName("rxValue");
            m_rxValue->setAlignment(Qt::AlignLeft);

            col->addWidget(key);
            col->addWidget(m_rxValue);
            metricsRow->addLayout(col);
        }

        metricsRow->addStretch();

        // TX
        {
            auto* col = new QVBoxLayout;
            auto* key = new QLabel("SENT", container);
            key->setObjectName("metricKey");

            m_txValue = new QLabel("0", container);
            m_txValue->setObjectName("txValue");
            m_txValue->setAlignment(Qt::AlignRight);

            col->addWidget(key);
            col->addWidget(m_txValue);
            metricsRow->addLayout(col);
        }

        root->addLayout(metricsRow);
    }

    root->addWidget(makeLine());

    {
        m_statusLabel = new QLabel("● Connecting", container);
        m_statusLabel->setObjectName("status");
        m_statusLabel->setAlignment(Qt::AlignCenter);
        root->addWidget(m_statusLabel);
    }

    // ── Shadow ─────────────────────────────────────────────────
    auto* shadow = new QGraphicsDropShadowEffect(container);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 60));
    container->setGraphicsEffect(shadow);
}

// ---- poller management ------------------------------------------------------
void StatusPanel::startPoller()
{
    if (m_thread)
        return;   // already running

    m_thread = new QThread(this);
    m_poller = new StatusPoller(m_configFile);   // no Qt parent — moved to thread
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
void StatusPanel::onStats(quint64 rx, quint64 tx)
{
    m_rxValue->setText(formatBytes(rx));
    m_txValue->setText(formatBytes(tx));
    m_statusLabel->clear();
}

void StatusPanel::onError(const QString& msg)
{
    m_rxValue->setText("—");
    m_txValue->setText("—");
    m_statusLabel->setText(msg.isEmpty() ? "Adapter unavailable" : msg);
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


// ============================================================
//  StatusPoller
// ============================================================
StatusPoller::StatusPoller(const QString& configFile, QObject* parent)
    : QObject(parent)
    , m_configFile(configFile)
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
#include <QDateTime>
QString StatusPoller::formatHandshake(quint64 lastHandshakeMsec) {
    if (lastHandshakeMsec == 0)
        return QObject::tr("Never");

    quint64 nowMsec = static_cast<quint64>(
        QDateTime::currentMSecsSinceEpoch());
    quint64 elapsedSec = (nowMsec - lastHandshakeMsec) / 1000;

    quint64 days    = elapsedSec / 86400;
    quint64 hours   = (elapsedSec % 86400) / 3600;
    quint64 minutes = (elapsedSec % 3600) / 60;
    quint64 seconds = elapsedSec % 60;

    QString result;
    if (days > 0)
        result += QString("%1 天%2, ").arg(days).arg(days == 1 ? "" : "s");
    if (hours > 0)
        result += QString("%1 时%2, ").arg(hours).arg(hours == 1 ? "" : "s");
    if (minutes > 0)
        result += QString("%1 分%2, ").arg(minutes).arg(minutes == 1 ? "" : "s");
    result += QString("%1 秒%2").arg(seconds).arg(seconds == 1 ? "" : "s");

    return result;
    //return result + QObject::tr(" 前");
}
void StatusPoller::poll()
{
    if (!m_running)
        return;

    try {
        auto adapter = Tunnel::Driver::Adapter::open(m_configFile.toStdWString());
        const Tunnel::Interface cfg = adapter.getConfiguration();

        quint64 rx = 0, tx = 0, lastHandshake=0;
        // only one peer for a client
        for (const auto& peer : cfg.peers) {
            rx += static_cast<quint64>(peer.rxBytes);
            tx += static_cast<quint64>(peer.txBytes);
            // Most recent handshake across all peers (tunnel is "alive" if any peer is active)
            lastHandshake = (std::max)(lastHandshake, static_cast<quint64>(peer.lastHandshakeMsec));
        }

        //spdlog::debug("lastHandshakeMsec: {}", formatHandshake(lastHandshake).toStdString());
        emit statsReady(rx, tx);
    }
    catch (const std::exception& ex) {
        emit errorOccurred(QString::fromStdString(ex.what()));
        spdlog::debug("StatusPoller::poll failed: {}", ex.what());
    }
}
