#pragma once
#include <shared_mutex>
#include <optional>
#include <string>
#include <unordered_map>

class LocalCache {
public:
    std::optional<std::string> tryGet(const std::string& key) const {
        std::shared_lock<std::shared_mutex> guard(mutex_);
        auto it = map_.find(key);
        if (it == map_.end()) return std::nullopt;
        return it->second;
    }

    void upsert(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> guard(mutex_);
        map_[key] = value;
    }

    void erase(const std::string& key) {
        std::unique_lock<std::shared_mutex> guard(mutex_);
        map_.erase(key);
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> map_;
};


