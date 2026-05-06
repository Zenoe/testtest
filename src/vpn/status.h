#pragma once
// ============================================================
//  TransferPanel.h
//  A floating panel that polls WireGuard rx/tx every second.
//  Usage:
//      auto* panel = new TransferPanel(configFile, parent);
//      panel->show();   // call from your button's clicked() slot
// ============================================================

#include <QWidget>
#include <QString>
#include <QThread>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <cstdint>

// Forward-declared poller lives on a worker thread
class TransferPoller;

// ============================================================
//  TransferPanel  –  the visible widget
// ============================================================
class TransferPanel : public QWidget
{
    Q_OBJECT

public:
    /// @param configFile  tunnel name passed to Adapter::open()
    /// @param parent      optional Qt parent (panel is a top-level window when nullptr)
    explicit TransferPanel(const QString& configFile, QWidget* parent = nullptr);
    ~TransferPanel() override;

    /// Call this from your button's clicked() slot.
    /// Starts polling and shows the panel (toggle behaviour).
    void toggle();

signals:
    /// Emitted by the poller (cross-thread) → connected to updateDisplay()
    void statsUpdated(quint64 rx, quint64 tx);
    /// Emitted when the adapter is unavailable
    void adapterError(const QString& message);

private slots:
    void updateDisplay(quint64 rx, quint64 tx);
    void onAdapterError(const QString& message);
    void onCloseClicked();

private:
    void buildUi();
    void startPoller();
    void stopPoller();
    static QString formatBytes(quint64 bytes);

    QString         m_configFile;

    // ---- UI elements ----
    QLabel*         m_rxLabel      = nullptr;
    QLabel*         m_txLabel      = nullptr;
    QLabel*         m_statusLabel  = nullptr;
    QPushButton*    m_closeBtn     = nullptr;

    // ---- Worker thread ----
    QThread*        m_thread       = nullptr;
    TransferPoller* m_poller       = nullptr;
};


// ============================================================
//  TransferPoller  –  lives on m_thread, never touches the UI
// ============================================================
class TransferPoller : public QObject
{
    Q_OBJECT

public:
    explicit TransferPoller(const QString& configFile, QObject* parent = nullptr);

public slots:
    void start();   ///< Called once the worker thread has started
    void stop();    ///< Requests clean shutdown

signals:
    void statsReady(quint64 rx, quint64 tx);
    void errorOccurred(const QString& message);

private:
    void poll();

    QString  m_configFile;
    bool     m_running = false;
    QTimer*  m_timer   = nullptr;
};
