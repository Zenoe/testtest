#pragma once
#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include "models/AppEntry.h"

class AppGridWidget : public QWidget {
    Q_OBJECT

public:
    explicit AppGridWidget(QWidget* parent = nullptr);

public slots:
    void onAppsReceived(const QList<AppEntry>& apps);

signals:
    void appLaunchRequested(const AppEntry& app);
    void logoutRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onAppActivated(const AppEntry& app);

private:
    void setupUi();
    void setupConnections();
    void rebuildGrid(const QList<AppEntry>& apps);
    void clearGrid();
    int  columnCount() const;       // dynamic, based on current widget width

    QScrollArea*  m_scrollArea;
    QWidget*      m_container;      // inside scroll area
    QGridLayout*  m_gridLayout;
    QLabel*       m_emptyLabel;     // shown when app list is empty
    QProgressBar* m_loadingBar;     // indeterminate, shown while fetching

    QList<AppEntry> m_apps;         // cached for resize reflow
};
