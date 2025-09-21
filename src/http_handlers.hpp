#pragma once
#include <boost/asio/awaitable.hpp>
#include <boost/beast/http.hpp>
#include <string>
#include "data_layer.hpp"

namespace http = boost::beast::http;
namespace net  = boost::asio;

struct ServerConfig {
    std::string apiKey; // from env KV_API_KEY
};

net::awaitable<http::response<http::string_body>>
handleGetKV(const http::request<http::string_body>& req, const std::string& key, DataLayer& data);

net::awaitable<http::response<http::string_body>>
handlePostKV(const http::request<http::string_body>& req, const std::string& key, const ServerConfig& cfg, DataLayer& data);


