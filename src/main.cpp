#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <cstdlib>
#include <thread>
#include <vector>
#include <string>
#include <iostream>
#include "cache.hpp"
#include "data_layer.hpp"
#include "http_handlers.hpp"

namespace net = boost::asio;
namespace http = boost::beast::http;
using tcp = net::ip::tcp;

struct App {
    ServerConfig serverCfg;
    RedisConfig redisCfg;
    LocalCache cache;
    std::unique_ptr<DataLayer> data;
};

net::awaitable<void> session(tcp::socket socket, App& app) {
    boost::beast::tcp_stream stream(std::move(socket));
    boost::beast::flat_buffer buffer;

    for (;;) {
        http::request<http::string_body> req;
        boost::system::error_code read_ec;
        co_await http::async_read(stream, buffer, req, net::redirect_error(net::use_awaitable, read_ec));
        if (read_ec) break;

        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.keep_alive(req.keep_alive());

        try {
            const std::string target = std::string(req.target());
            if (req.method() == http::verb::get && (target == "/" || target == "/health")) {
                http::response<http::string_body> ok{http::status::ok, req.version()};
                ok.set(http::field::content_type, "text/plain; charset=utf-8");
                ok.body() = "json-kv-service";
                ok.prepare_payload();
                ok.keep_alive(req.keep_alive());
                res = std::move(ok);
            } else if (target.rfind("/kv/", 0) == 0) {
                auto key = target.substr(4);
                if (req.method() == http::verb::get) {
                    res = co_await handleGetKV(req, key, *app.data);
                } else if (req.method() == http::verb::post) {
                    res = co_await handlePostKV(req, key, app.serverCfg, *app.data);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[request-error] " << e.what() << '\n';
            http::response<http::string_body> err{http::status::internal_server_error, req.version()};
            err.set(http::field::content_type, "text/plain; charset=utf-8");
            err.keep_alive(false);
            err.body() = "internal error";
            err.prepare_payload();
            res = std::move(err);
        } catch (...) {
            std::cerr << "[request-error] unknown" << '\n';
            http::response<http::string_body> err{http::status::internal_server_error, req.version()};
            err.set(http::field::content_type, "text/plain; charset=utf-8");
            err.keep_alive(false);
            err.body() = "internal error";
            err.prepare_payload();
            res = std::move(err);
        }

        co_await http::async_write(stream, res, net::use_awaitable);
        if (!res.keep_alive()) break;
    }

    boost::system::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_send, ec);
    co_return;
}

net::awaitable<void> listener(tcp::endpoint ep, App& app) {
    tcp::acceptor acceptor(co_await net::this_coro::executor, ep);
    for (;;) {
        tcp::socket socket = co_await acceptor.async_accept(net::use_awaitable);
        net::co_spawn(acceptor.get_executor(), session(std::move(socket), app), net::detached);
    }
}

int main(int argc, char** argv) {
    const char* apiKey = std::getenv("KV_API_KEY");
    if (!apiKey) {
        std::fprintf(stderr, "KV_API_KEY not set\n");
        return 1;
    }

    unsigned short port = 8001;
    if (const char* envPort = std::getenv("PORT")) {
        port = static_cast<unsigned short>(std::atoi(envPort));
    } else if (argc > 1) {
        port = static_cast<unsigned short>(std::atoi(argv[1]));
    }

    net::io_context io;
    RedisConfig rcfg;
    if (const char* rh = std::getenv("REDIS_HOST")) rcfg.host = rh;
    if (const char* rp = std::getenv("REDIS_PORT")) rcfg.port = rp;

    App app{
        .serverCfg = ServerConfig{.apiKey = apiKey},
        .redisCfg = rcfg
    };
    app.data = std::make_unique<DataLayer>(io, app.cache, app.redisCfg);

    net::co_spawn(io, app.data->runSubscriber(), net::detached);

    const char* bindHostEnv = std::getenv("BIND_HOST");
    std::string bindHost = bindHostEnv ? bindHostEnv : "0.0.0.0";
    auto addr = net::ip::make_address(bindHost);
    net::co_spawn(io, listener(tcp::endpoint{addr, port}, app), net::detached);

    const unsigned numThreads = std::max(2u, std::thread::hardware_concurrency());
    std::vector<std::thread> workers;
    workers.reserve(numThreads - 1);
    for (unsigned i = 0; i + 1 < numThreads; ++i) workers.emplace_back([&]{ io.run(); });
    io.run();
    for (auto& t : workers) t.join();
    return 0;
}


