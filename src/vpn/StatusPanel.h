#pragma once
// ============================================================
//  StatusPanel.h
//  Polls WireGuard rx/tx every second and displays it.
//
//  Usage in MainWindow:
//      // construction (once, in MainWindow ctor)
//      m_panel = new StatusPanel(configFile, this);
//
//      // wire to your button
//      connect(ui->statsButton, &QPushButton::clicked,
//              m_panel, &StatusPanel::toggle);
// ============================================================

#include <QWidget>
#include <QThread>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QString>
#include <cstdint>

class StatusPoller;

// ============================================================
//  StatusPanel
// ============================================================
class StatusPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StatusPanel(const QString& configFile, QWidget* parent = nullptr);
    ~StatusPanel() override;

public slots:
    /// Toggle visibility + poller. Wire directly to QPushButton::clicked.
    void toggle();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;


private slots:
    void onStats(quint64 rx, quint64 tx);
    void onError(const QString& msg);

private:
    void buildUi();
    void startPoller();
    void stopPoller();
    static QString formatBytes(quint64 bytes);

    QString          m_configFile;

    QLabel*          m_rxValue     = nullptr;
    QLabel*          m_txValue     = nullptr;
    QLabel*          m_statusLabel = nullptr;

    QThread*         m_thread      = nullptr;
    StatusPoller*  m_poller      = nullptr;
};

// ============================================================
//  StatusPoller  – lives on background thread
// ============================================================
class StatusPoller : public QObject
{
    Q_OBJECT

public:
    explicit StatusPoller(const QString& configFile, QObject* parent = nullptr);

public slots:
    void start();
    void stop();

signals:
    void statsReady(quint64 rx, quint64 tx);
    void errorOccurred(const QString& msg);

private slots:
    void poll();

private:
    QString  m_configFile;
    QTimer*  m_timer   = nullptr;
    bool     m_running = false;
};
