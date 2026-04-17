#pragma once
#include <QApplication>

// Subclass of QApplication — centralises startup, stylesheet loading,
// crash handling, and singleton teardown order.
class Application : public QApplication {
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

    // Call after construction to initialise all backend singletons
    void initialise();

private:
    void loadStylesheet();
    void configureHighDpi();
};
