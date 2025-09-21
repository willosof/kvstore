#include "http_handlers.hpp"
#include <boost/beast/core.hpp>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

static std::string strongEtagFor(const std::string& s) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(s.data()), s.size(), hash);
    std::ostringstream os;
    for (unsigned char c : hash) os << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    return "\"" + os.str() + "\"";
}

net::awaitable<http::response<http::string_body>>
handleGetKV(const http::request<http::string_body>& req, const std::string& key, DataLayer& data) {
    auto value = co_await data.getValue(key);
    if (!value) {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.keep_alive(req.keep_alive());
        res.content_length(0);
        co_return res;
    }

    auto etag = strongEtagFor(*value);
    auto inm = req[http::field::if_none_match];
    if (!inm.empty() && inm == etag) {
        http::response<http::string_body> res{http::status::not_modified, req.version()};
        res.set(http::field::etag, etag);
        res.set(http::field::cache_control, "no-cache");
        res.keep_alive(req.keep_alive());
        res.content_length(0);
        co_return res;
    }

    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-cache");
    res.set(http::field::etag, etag);
    res.body() = *value;
    res.prepare_payload();
    res.keep_alive(req.keep_alive());
    co_return res;
}

net::awaitable<http::response<http::string_body>>
handlePostKV(const http::request<http::string_body>& req, const std::string& key, const ServerConfig& cfg, DataLayer& data) {
    auto api = req["X-API-Key"];
    if (api != cfg.apiKey) {
        http::response<http::string_body> res{http::status::forbidden, req.version()};
        res.keep_alive(req.keep_alive());
        res.content_length(0);
        co_return res;
    }

    // Expect the body to be a JSON object string as-is.
    const std::string jsonBody = req.body();
    co_await data.setValue(key, jsonBody);

    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/json");
    res.body() = "{\"ok\":true}";
    res.prepare_payload();
    res.keep_alive(req.keep_alive());
    co_return res;
}


