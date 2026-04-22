#include "SettingsDialog.h"
#include <QListWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QFormLayout>

#include "utils/ConfigManager.h"

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("设置");
    resize(800, 500);

    auto* mainLayout = new QHBoxLayout(this);

    // ───────── 左侧导航 ─────────
    m_navList = new QListWidget;
    m_navList->addItem("通用");
    m_navList->addItem("服务器设置");

    m_navList->setFixedWidth(150);

    // ───────── 右侧页面 ─────────
    m_stack = new QStackedWidget;

    m_stack->addWidget(createGeneralPage());
    m_stack->addWidget(createServerPage());

    mainLayout->addWidget(m_navList);
    mainLayout->addWidget(m_stack);

    connect(m_navList, &QListWidget::currentRowChanged,
        m_stack, &QStackedWidget::setCurrentIndex);

    m_navList->setCurrentRow(0);
}

QWidget* SettingsDialog::createGeneralPage() {
    QWidget* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    QLabel* title = new QLabel("通用设置");
    title->setStyleSheet("font-size:18px; font-weight:bold;");

    QCheckBox* autoStart = new QCheckBox("开机自动启动");

    // 🔥 从配置读取
    bool enabled = ConfigManager::instance().get<bool>("general.autostart", false);
    autoStart->setChecked(enabled);

    // 🔥 写回配置
    connect(autoStart, &QCheckBox::toggled, [](bool checked) {
        ConfigManager::instance().set("general.autostart", checked);
        });

    layout->addWidget(title);
    layout->addSpacing(10);
    layout->addWidget(autoStart);
    layout->addStretch();

    return page;
}

QWidget* SettingsDialog::createServerPage() {
    QWidget* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    QLabel* title = new QLabel("服务器设置");
    title->setStyleSheet("font-size:18px; font-weight:bold;");

    auto* form = new QFormLayout;

    QLineEdit* ipEdit = new QLineEdit;
    QSpinBox* portEdit = new QSpinBox;

    portEdit->setRange(1, 65535);

    // 🔥 从配置加载
    ipEdit->setText(QString::fromStdString(
        ConfigManager::instance().get<std::string>("server.host", "127.0.0.1")
    ));

    portEdit->setValue(
        ConfigManager::instance().get<int>("server.port", 51820)
    );

    // 🔥 自动保存（实时）
    connect(ipEdit, &QLineEdit::textChanged, [](const QString& text) {
        ConfigManager::instance().set("server.host", text.toStdString());
        });

    connect(portEdit, QOverload<int>::of(&QSpinBox::valueChanged), [](int val) {
        ConfigManager::instance().set("server.port", val);
        });

    form->addRow("服务器地址:", ipEdit);
    form->addRow("端口:", portEdit);

    layout->addWidget(title);
    layout->addSpacing(10);
    layout->addLayout(form);
    layout->addStretch();

    return page;
}