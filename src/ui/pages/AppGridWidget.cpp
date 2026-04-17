#include "AppGridWidget.h"
#include "ui/components/AppItemWidget.h"
#include <QVBoxLayout>
#include <QResizeEvent>

AppGridWidget::AppGridWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void AppGridWidget::setupUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Loading bar (indeterminate, shown while fetching app list)
    m_loadingBar = new QProgressBar(this);
    m_loadingBar->setObjectName("AppGridLoadingBar");
    m_loadingBar->setFixedHeight(3);
    m_loadingBar->setRange(0, 0);
    m_loadingBar->setTextVisible(false);
    m_loadingBar->setVisible(false);
    root->addWidget(m_loadingBar);

    // Scroll area wraps the grid container
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("AppGridScroll");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Container widget that holds the actual grid
    m_container = new QWidget(m_scrollArea);
    m_container->setObjectName("AppGridContainer");

    m_gridLayout = new QGridLayout(m_container);
    m_gridLayout->setContentsMargins(16, 16, 16, 16);
    m_gridLayout->setSpacing(8);
    m_gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_scrollArea->setWidget(m_container);
    root->addWidget(m_scrollArea, 1);

    // Empty state label (shown when app list is empty after load)
    m_emptyLabel = new QLabel("没有可用的应用程序", this);
    m_emptyLabel->setObjectName("EmptyLabel");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setVisible(false);
    root->addWidget(m_emptyLabel, 1, Qt::AlignCenter);
}

void AppGridWidget::setupConnections() {
    // Nothing to wire at construction time —
    // AppItemWidget connections are created dynamically in rebuildGrid()
}

// ── Public slots ──────────────────────────────────────────────────────────────

void AppGridWidget::onAppsReceived(const QList<AppEntry>& apps) {
    m_loadingBar->setVisible(false);
    m_apps = apps;                      // cache for resize reflow

    if (apps.isEmpty()) {
        m_scrollArea->setVisible(false);
        m_emptyLabel->setVisible(true);
        return;
    }

    m_emptyLabel->setVisible(false);
    m_scrollArea->setVisible(true);
    rebuildGrid(apps);
}

// ── Private slots ─────────────────────────────────────────────────────────────

void AppGridWidget::onAppActivated(const AppEntry& app) {
    emit appLaunchRequested(app);
}

// ── Private helpers ───────────────────────────────────────────────────────────

void AppGridWidget::clearGrid() {
    // Remove and delete every item currently in the grid
    while (m_gridLayout->count() > 0) {
        QLayoutItem* item = m_gridLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

int AppGridWidget::columnCount() const {
    // Each tile is 100px wide + 8px gap; minimum 1 column
    const int available = m_scrollArea->viewport()->width() - 32; // minus margins
    return qMax(1, (available + 8) / (100 + 8));
}

void AppGridWidget::rebuildGrid(const QList<AppEntry>& apps) {
    clearGrid();

    const int cols = columnCount();
    int row = 0, col = 0;

    for (const AppEntry& app : apps) {
        auto* tile = new AppItemWidget(app, m_container);
        connect(tile, &AppItemWidget::activated,
                this, &AppGridWidget::onAppActivated);
        m_gridLayout->addWidget(tile, row, col);

        if (++col >= cols) {
            col = 0;
            ++row;
        }
    }

    // Fill remaining cells in the last row with invisible spacers
    // so tiles left-align rather than stretching to fill the row
    while (col > 0 && col < cols) {
        auto* spacer = new QWidget(m_container);
        spacer->setFixedSize(100, 100);
        m_gridLayout->addWidget(spacer, row, col++);
    }
}

// ── Protected events ──────────────────────────────────────────────────────────

void AppGridWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    // Reflow grid if the column count changes after resize
    if (!m_apps.isEmpty()) {
        const int newCols = columnCount();

        // Count current non-spacer tiles
        int tileCols = 0;
        for (int i = 0; i < m_gridLayout->count(); ++i) {
            auto* w = m_gridLayout->itemAt(i)->widget();
            if (qobject_cast<AppItemWidget*>(w))
                ++tileCols;
            else
                break;      // hit the spacer region — stop counting
        }
        // Only rebuild when column count actually changes to avoid flicker
        if (newCols != tileCols)
            rebuildGrid(m_apps);
    }
}
