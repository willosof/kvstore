#include "data_layer.hpp"
#include <boost/redis.hpp>
#include <boost/redis/adapter/adapt.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <iostream>

using namespace std::literals;

DataLayer::DataLayer(net::io_context& io, LocalCache& cache, const RedisConfig& cfg)
    : io_(io), cache_(cache), cfg_(cfg) {

    redis::config rc;
    rc.addr.host = cfg_.host;
    rc.addr.port = cfg_.port;

    cmdConn_ = std::make_shared<redis::connection>(io_);
    subConn_ = std::make_shared<redis::connection>(io_);

    cmdConn_->async_run(rc, net::detached);
    subConn_->async_run(rc, net::detached);
}

net::awaitable<std::optional<std::string>> DataLayer::getValue(const std::string& key) {
    if (auto v = cache_.tryGet(key)) co_return v;

    redis::request req;
    req.push("GET", key);

    redis::response<std::optional<std::string>> resp;
    co_await cmdConn_->async_exec(req, resp, net::use_awaitable);

    auto r0 = std::get<0>(resp);
    if (!r0.has_value()) co_return std::nullopt;
    auto valueOpt = r0.value();
    if (!valueOpt) co_return std::nullopt;

    if (!cache_.tryGet(key)) cache_.upsert(key, *valueOpt);
    co_return valueOpt;
}

net::awaitable<void> DataLayer::setValue(const std::string& key, const std::string& jsonValue) {
    redis::request req;
    req.push("SET", key, jsonValue);
    req.push("PUBLISH", cfg_.channel, key);

    redis::response<redis::ignore_t, long long> resp;
    co_await cmdConn_->async_exec(req, resp, net::use_awaitable);

    auto publishedResult = std::get<1>(resp);
    std::cout << "[own-update] key=" << key
              << " bytes=" << jsonValue.size();
    if (publishedResult.has_value()) {
        std::cout << " published=" << publishedResult.value() << '\n';
    } else {
        std::cout << " publish_error" << '\n';
    }

    cache_.upsert(key, jsonValue);
    co_return;
}

net::awaitable<void> DataLayer::runSubscriber() {
    redis::request subReq;
    subReq.push("SUBSCRIBE", cfg_.channel);
    redis::response<redis::ignore_t> subResp;
    co_await subConn_->async_exec(subReq, subResp, net::use_awaitable);

    redis::generic_response push;
    subConn_->set_receive_response(push);

    for (;;) {
        // Clear previous push contents and await next push
        push = redis::generic_response{};
        co_await subConn_->async_receive(net::use_awaitable);

        auto const& nodes = push.value();
        for (std::size_t i = 0; i + 2 < nodes.size(); ++i) {
            if (nodes[i].value == "message") {
                std::string key(nodes[i + 2].value);
                if (!key.empty()) {
                    std::cout << "[incoming-update] key=" << key << " -> invalidate cache" << '\n';
                    cache_.erase(key);
                }
            }
        }
    }
}


