#pragma once

/*

if (cfg.get("server", "") != "") // error

using namespace std::string_literals;
// The "s" suffix makes this a std::string object, not a const char*
if (cfg.get("server", ""s) != ""s) {
    NetworkManager::instance().setServer(cfg.get<std::string>("server", ""));
}
Option B: Explicit Template Argument
// Explicitly tell the compiler T is std::string
if (cfg.get<std::string>("server", "") != "") {
    NetworkManager::instance().setServer(cfg.get<std::string>("server", ""));
}
*/
#include <nlohmann/json.hpp>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

class ConfigManager {
public:
    static ConfigManager& instance();

    void init(const std::string& filePath);

    // Getters
    template<typename T>
    T get(const std::string& key, const T& defaultValue);

    // Setters
    template<typename T>
    void set(const std::string& key, const T& value);

    void saveNow();   // force save
    void shutdown();  // clean shutdown

    bool contains(const std::string& key);
    nlohmann::json getSection(const std::string& key);

private:
    ConfigManager();
    ~ConfigManager();

    void load();
    void saveToDisk();

    // background worker
    void workerThread();

    // helpers
    nlohmann::json* getJsonNode(const std::string& key, bool create);

private:
    std::string m_filePath;
    nlohmann::json m_json;

    std::mutex m_mutex;

    std::thread m_worker;
    std::condition_variable m_cv;

    std::atomic<bool> m_dirty{false};
    std::atomic<bool> m_exit{false};
};
