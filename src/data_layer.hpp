#pragma once
#include <boost/redis/connection.hpp>
#include <boost/asio/awaitable.hpp>
#include <optional>
#include <string>
#include <memory>
#include "cache.hpp"

namespace net = boost::asio;
namespace redis = boost::redis;

struct RedisConfig {
    std::string host = "127.0.0.1";
    std::string port = "6379";
    std::string channel = "kv_updates";
};

class DataLayer {
public:
    DataLayer(net::io_context& io, LocalCache& cache, const RedisConfig& cfg);

    net::awaitable<std::optional<std::string>> getValue(const std::string& key);
    net::awaitable<void> setValue(const std::string& key, const std::string& jsonValue);
    net::awaitable<void> runSubscriber(); // long-running task

private:
    net::io_context& io_;
    LocalCache& cache_;
    RedisConfig cfg_;
    std::shared_ptr<redis::connection> cmdConn_;
    std::shared_ptr<redis::connection> subConn_;
};


