#pragma once
#include <QDialog>

class QListWidget;
class QStackedWidget;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private:
    QListWidget* m_navList;
    QStackedWidget* m_stack;

    QWidget* createGeneralPage();
    QWidget* createServerPage();
};