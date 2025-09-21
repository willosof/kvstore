#pragma once
#include <atomic>
#include <string>
#include <sstream>

class Metrics {
public:
    // HTTP
    void incHttpTotal() { http_requests_total_.fetch_add(1, std::memory_order_relaxed); }
    void incHttpRouteHealth() { http_requests_health_.fetch_add(1, std::memory_order_relaxed); }
    void incHttpRouteMetrics() { http_requests_metrics_.fetch_add(1, std::memory_order_relaxed); }
    void incHttpRouteKvGet() { http_requests_kv_get_.fetch_add(1, std::memory_order_relaxed); }
    void incHttpRouteKvPost() { http_requests_kv_post_.fetch_add(1, std::memory_order_relaxed); }
    void incHttpRouteOther() { http_requests_other_.fetch_add(1, std::memory_order_relaxed); }

    // KV GET/SET
    void incKvGetRequests() { kv_get_requests_.fetch_add(1, std::memory_order_relaxed); }
    void incKvGetHits() { kv_get_hits_.fetch_add(1, std::memory_order_relaxed); }
    void incKvGetMisses() { kv_get_misses_.fetch_add(1, std::memory_order_relaxed); }
    void incKvSourceCache() { kv_source_cache_.fetch_add(1, std::memory_order_relaxed); }
    void incKvSourceRedis() { kv_source_redis_.fetch_add(1, std::memory_order_relaxed); }
    void incKvSetRequests() { kv_set_requests_.fetch_add(1, std::memory_order_relaxed); }

    // Cache invalidations (from subscriber)
    void incCacheInvalidations() { cache_invalidations_.fetch_add(1, std::memory_order_relaxed); }

    // Redis health
    void incRedisPingSuccess() { redis_ping_success_.fetch_add(1, std::memory_order_relaxed); }
    void incRedisPingFailure() { redis_ping_failure_.fetch_add(1, std::memory_order_relaxed); }

    std::string renderPrometheus() const {
        std::ostringstream os;

        // HTTP
        os << "# TYPE http_requests_total counter\n";
        os << "http_requests_total " << http_requests_total_.load(std::memory_order_relaxed) << "\n";
        os << "http_requests_total{route=\"health\"} " << http_requests_health_.load(std::memory_order_relaxed) << "\n";
        os << "http_requests_total{route=\"metrics\"} " << http_requests_metrics_.load(std::memory_order_relaxed) << "\n";
        os << "http_requests_total{route=\"kv_get\"} " << http_requests_kv_get_.load(std::memory_order_relaxed) << "\n";
        os << "http_requests_total{route=\"kv_post\"} " << http_requests_kv_post_.load(std::memory_order_relaxed) << "\n";
        os << "http_requests_total{route=\"other\"} " << http_requests_other_.load(std::memory_order_relaxed) << "\n";

        // KV
        os << "# TYPE kv_get_requests_total counter\n";
        os << "kv_get_requests_total " << kv_get_requests_.load(std::memory_order_relaxed) << "\n";

        os << "# TYPE kv_get_hits_total counter\n";
        os << "kv_get_hits_total " << kv_get_hits_.load(std::memory_order_relaxed) << "\n";

        os << "# TYPE kv_get_misses_total counter\n";
        os << "kv_get_misses_total " << kv_get_misses_.load(std::memory_order_relaxed) << "\n";

        os << "# TYPE kv_source_total counter\n";
        os << "kv_source_total{source=\"cache\"} " << kv_source_cache_.load(std::memory_order_relaxed) << "\n";
        os << "kv_source_total{source=\"redis\"} " << kv_source_redis_.load(std::memory_order_relaxed) << "\n";

        os << "# TYPE kv_set_requests_total counter\n";
        os << "kv_set_requests_total " << kv_set_requests_.load(std::memory_order_relaxed) << "\n";

        os << "# TYPE cache_invalidations_total counter\n";
        os << "cache_invalidations_total " << cache_invalidations_.load(std::memory_order_relaxed) << "\n";

        os << "# TYPE redis_ping_total counter\n";
        os << "redis_ping_total{status=\"success\"} " << redis_ping_success_.load(std::memory_order_relaxed) << "\n";
        os << "redis_ping_total{status=\"failure\"} " << redis_ping_failure_.load(std::memory_order_relaxed) << "\n";

        return os.str();
    }

private:
    // HTTP
    std::atomic<unsigned long long> http_requests_total_{0};
    std::atomic<unsigned long long> http_requests_health_{0};
    std::atomic<unsigned long long> http_requests_metrics_{0};
    std::atomic<unsigned long long> http_requests_kv_get_{0};
    std::atomic<unsigned long long> http_requests_kv_post_{0};
    std::atomic<unsigned long long> http_requests_other_{0};

    // KV
    std::atomic<unsigned long long> kv_get_requests_{0};
    std::atomic<unsigned long long> kv_get_hits_{0};
    std::atomic<unsigned long long> kv_get_misses_{0};
    std::atomic<unsigned long long> kv_source_cache_{0};
    std::atomic<unsigned long long> kv_source_redis_{0};
    std::atomic<unsigned long long> kv_set_requests_{0};

    // Cache invalidation
    std::atomic<unsigned long long> cache_invalidations_{0};

    // Redis
    std::atomic<unsigned long long> redis_ping_success_{0};
    std::atomic<unsigned long long> redis_ping_failure_{0};
};


