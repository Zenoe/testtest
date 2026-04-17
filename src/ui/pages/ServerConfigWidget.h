// ui/pages/ServerConfigWidget.h
#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include "models/ServerConfig.h"

class ServerConfigWidget : public QWidget {
    Q_OBJECT
public:
    explicit ServerConfigWidget(QWidget* parent = nullptr);

signals:
    void configSaved(const ServerConfig& cfg);
    void testRequested();

public slots:
    void onTestResult(bool ok, const QString& msg);

protected:
    void resizeEvent(QResizeEvent* event) override;
private slots:
    void onOkClicked();
    void onTestClicked();
    void validateInputs();

private:
    void setupUi();
    ServerConfig currentConfig() const;
    QLabel*  m_bgLabel   = nullptr;
    QWidget* m_formPanel = nullptr;
    QLineEdit*   m_hostEdit;
    QLineEdit*   m_portEdit;
    QComboBox*   m_groupCombo;
    QPushButton* m_testBtn;
    QPushButton* m_okBtn;
    QLabel*      m_statusLabel;
};
