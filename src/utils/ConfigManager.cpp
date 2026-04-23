#include "ConfigManager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

using json = nlohmann::json;

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {}

ConfigManager::~ConfigManager() {
    shutdown();
}

void ConfigManager::init(const std::string& filePath) {
    m_filePath = filePath;

    load();

    // start background thread
    m_worker = std::thread(&ConfigManager::workerThread, this);
}

void ConfigManager::shutdown() {
    m_exit = true;
    m_cv.notify_all();

    if (m_worker.joinable())
        m_worker.join();

    saveNow();
}

void ConfigManager::load() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream f(m_filePath);
    if (!f) {
        m_json = json::object();
        m_json["version"] = 1;
        return;
    }

    try {
        f >> m_json;
    } catch (...) {
        m_json = json::object();
        m_json["version"] = 1;
    }
}

void ConfigManager::saveToDisk() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::filesystem::create_directories(
        std::filesystem::path(m_filePath).parent_path()
    );

    std::ofstream f(m_filePath);
    f << m_json.dump(4);
}

void ConfigManager::saveNow() {
    saveToDisk();
    m_dirty = false;
}

void ConfigManager::workerThread() {
    std::unique_lock<std::mutex> lock(m_mutex);

    while (!m_exit) {
        m_cv.wait_for(lock, std::chrono::seconds(2));

        if (m_dirty) {
            lock.unlock();
            saveToDisk();
            m_dirty = false;
            lock.lock();
        }
    }
}

json* ConfigManager::getJsonNode(const std::string& key, bool create) {
    nlohmann::json* current = &m_json;
    if (key.empty()) return current;

    std::stringstream ss(key);
    std::string token;

    while (std::getline(ss, token, '.')) {
        if (token.empty()) continue; // Skip empty tokens from "double..dots"

        // Safety check: ensure we can actually nest inside 'current'
        if (!current->is_object()) {
            if (create) {
                *current = nlohmann::json::object();
            }
            else {
                return nullptr;
            }
        }

        if (!current->contains(token)) {
            if (create) {
                (*current)[token] = nlohmann::json::object();
            }
            else {
                return nullptr;
            }
        }

        current = &((*current)[token]);
    }

    return current;
}
//json* ConfigManager::getJsonNode(const std::string& key, bool create) {
//    std::stringstream ss(key);
//    std::string item;
//
//    json* current = &m_json;
//
//    while (std::getline(ss, item, '.')) {
//        if (!current->contains(item)) {
//            if (create)
//                (*current)[item] = json::object();
//            else
//                return nullptr;
//        }
//        current = &((*current)[item]);
//    }
//
//    return current;
//}
template<typename T>
std::optional<T> ConfigManager::get(const std::string& key)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    json* node = getJsonNode(key, false);
    if (!node || node->is_null())
        return std::nullopt;

    try {
		// without this, if the stored value is a number but we ask for a string, it will throw instead of converting
		if constexpr (std::is_same_v<T, std::string>) {
			if (node->is_string())
				return node->get<std::string>();
			if (node->is_number())
				return std::to_string(node->get<int>());
		}
		return node->get<T>();
	}
	catch (...) {
		return std::nullopt;
	}
}

template<typename T>
T ConfigManager::get(const std::string& key, const T& defaultValue)
{
    if (auto opt = get<T>(key))   // calls the optional version
        return *opt;

    return defaultValue;
}

template<typename T>
void ConfigManager::set(const std::string& key, const T& value) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        json* node = getJsonNode(key, true);
        *node = value;

        m_dirty = true;
    }

    m_cv.notify_all();
}

bool ConfigManager::contains(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Use the helper with create = false
    nlohmann::json* node = getJsonNode(key, false);
    return (node != nullptr && !node->is_discarded());
}

// Returns the raw JSON node, or an empty object if not found
nlohmann::json ConfigManager::getSection(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json* node = getJsonNode(key, false);

    if (node) {
        return *node; // Returns a copy of the sub-section
    }
    return nlohmann::json::object(); // Return empty object if missing
}
// Explicit template instantiations (for cpp split)
template int ConfigManager::get<int>(const std::string&, const int&);
template std::string ConfigManager::get<std::string>(const std::string&, const std::string&);
template bool ConfigManager::get<bool>(const std::string&, const bool&);

template void ConfigManager::set<int>(const std::string&, const int&);
template void ConfigManager::set<std::string>(const std::string&, const std::string&);
template void ConfigManager::set<bool>(const std::string&, const bool&);

template void ConfigManager::set<nlohmann::json>(const std::string&, const nlohmann::json&);
template nlohmann::json ConfigManager::get<nlohmann::json>(const std::string&, const nlohmann::json&);
