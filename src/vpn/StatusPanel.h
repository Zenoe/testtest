#pragma once
// ============================================================
//  StatusPanel.h
//  Polls WireGuard rx/tx every second and displays it.
//
//  Usage in MainWindow:
//      // construction (once, in MainWindow ctor)
//      m_panel = new StatusPanel(adapterName, configFile, this);
//
//      // wire to your button
//      connect(ui->statsButton, &QPushButton::clicked,
//              m_panel, &StatusPanel::toggle);
// ============================================================

#include <QWidget>
#include <QThread>
#include <QLabel>
#include <QPushButton>
#include <QPointer>
#include <QTimer>
#include <QString>
#include <cstdint>

class StatusPoller;
class RingLogDialog;

// ============================================================
//  StatusPanel
// ============================================================
class StatusPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StatusPanel(const QString& adapterName, const QString& configPath,
                         QWidget* parent = nullptr);
    ~StatusPanel() override;

public slots:
    /// Toggle visibility + poller. Wire directly to QPushButton::clicked.
    void toggle();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;


private slots:
    void onStats(quint64 rx, quint64 tx, qint64 lastHandshakeMsec);
    void onError(const QString& msg);
    void showRingLog();

private:
    void buildUi();
    void startPoller();
    void stopPoller();
    static QString formatBytes(quint64 bytes);
    static QString formatHandshake(qint64 lastHandshakeMsec);
    void setConnectionState(const QString& text, const QString& state);

    QString          m_adapterName;
    QString          m_configPath;

    QLabel*          m_rxValue     = nullptr;
    QLabel*          m_txValue     = nullptr;
    QLabel*          m_statusLabel = nullptr;
    QLabel*          m_handshakeValue = nullptr;

    QThread*         m_thread      = nullptr;
    StatusPoller*  m_poller      = nullptr;
    QPointer<RingLogDialog> m_ringLogDialog;
};

// ============================================================
//  StatusPoller  – lives on background thread
// ============================================================
class StatusPoller : public QObject
{
    Q_OBJECT

public:
    explicit StatusPoller(const QString& adapterName, QObject* parent = nullptr);

public slots:
    void start();
    void stop();

signals:
    void statsReady(quint64 rx, quint64 tx, qint64 lastHandshakeMsec);
    void errorOccurred(const QString& msg);

private slots:
    void poll();

private:
    QString  m_adapterName;
    QTimer*  m_timer   = nullptr;
    bool     m_running = false;
    quint32  m_consecutiveFailures = 0;
    QString  m_lastError;
};
