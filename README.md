## KISS Key Value Store

A minimal asynchronous JSON key-value HTTP service backed by Redis with a local in-memory cache and pub/sub invalidation.

### Architecture Overview
```mermaid
flowchart LR
  Client[[HTTP Client]]
  subgraph Service[json-kv-service]
    Router[HTTP Router - Boost.Beast]
    Handlers[Handlers]
    GetKV[GET /kv/:key]
    PostKV[POST /kv/:key]
    Data[DataLayer]
    Cache[LocalCache - shared_mutex + unordered_map]
  end
  Redis[(Redis)]

  Client -->|HTTP| Router
  Router --> Handlers
  Handlers --> GetKV
  Handlers --> PostKV
  GetKV --> Data
  PostKV --> Data
  Data --> Redis
  Redis --> Data
  Data -->|upsert/tryGet| Cache
  Data -->|PUBLISH kv_updates| Redis
```

- **Router**: parses requests and dispatches to handlers.
- **DataLayer**: mediates reads/writes to Redis, maintains the process-local cache, and runs a subscriber coroutine.
- **LocalCache**: thread-safe in-memory map using `shared_mutex` for read-heavy performance.

### Request Routing and Endpoints
```mermaid
sequenceDiagram
  participant C as Client
  participant R as Router (session)
  participant H as Handlers
  participant D as DataLayer
  participant Re as Redis
  participant Ca as LocalCache

  Note over R: GET / -> 200 "json-kv-service"
  Note over R: GET /health -> 200 "json-kv-service"

  C->>R: GET /kv/{key} (If-None-Match?)
  R->>H: handleGetKV(req, key)
  H->>D: getValue(key)
  D->>Ca: tryGet(key)
  alt cache hit
    Ca-->>D: value
  else cache miss
    D->>Re: GET key
    Re-->>D: bulk-string | nil
    alt found
      D->>Ca: upsert(key, value)
    else not found
      D-->>H: null
    end
  end
  alt not found
    H-->>C: 404
  else found
    H->>H: strong ETag(value)
    alt If-None-Match matches
      H-->>C: 304 Not Modified
    else no match
      H-->>C: 200 application/json + ETag
    end
  end

  C->>R: POST /kv/{key} body=json, X-API-Key
  R->>H: handlePostKV(req, key)
  H->>H: check X-API-Key == KV_API_KEY
  alt invalid
    H-->>C: 403
  else valid
    H->>D: setValue(key, json)
    D->>Re: SET key json
    D->>Re: PUBLISH kv_updates key
    D->>Ca: upsert(key, json)
    H-->>C: 200 {"ok":true}
  end
```

- **GET /** or **GET /health**: returns `json-kv-service`.
- **GET /kv/:key**: returns JSON value or 404; supports strong `ETag` + `If-None-Match` for conditional GETs; `Cache-Control: no-cache` signals revalidation.
- **POST /kv/:key**: writes raw JSON string; requires header `X-API-Key` matching `KV_API_KEY`; returns `{ "ok": true }`.

### Cache Behavior and Invalidation
```mermaid
flowchart TD
  A[GET /kv/:key] --> B{LocalCache has key?}
  B -- yes --> C[Return cached value]
  B -- no --> D[Redis GET]
  D -->|found| E[Cache upsert key value then return]
  D -->|nil| F[404]

  G[POST /kv/:key] --> H[Redis SET]
  H --> I[Redis PUBLISH kv_updates]
  H --> J[Cache upsert key value]

  subgraph Subscriber
    S[runSubscriber SUBSCRIBE kv_updates]
  end
  I --> S
```

Notes:
- Cache is warmed on GET misses and on successful POSTs.
- A long-running subscriber coroutine (`runSubscriber`) subscribes to `kv_updates`; current code reads messages but ignores payloads. This is a hook for multi-replica cache invalidation if you add erase/refresh logic.

### Configuration
- **Env**: `KV_API_KEY` (required), `PORT` (default 8001), `BIND_HOST` (default 0.0.0.0), `REDIS_HOST` (default 127.0.0.1), `REDIS_PORT` (default 6379)
- **Docker**: see `docker-compose.yml` for `redis` and `app` services; the app publishes to channel `kv_updates`.

### Example Requests
```bash
# Health
curl -s http://localhost:8001/health

# Write (replace API key)
curl -s -X POST http://localhost:8001/kv/user:42 \
  -H "X-API-Key: change_me" \
  -H "Content-Type: application/json" \
  --data '{"name":"Ada","tier":"pro"}'

# Read with conditional GET
etag=$(curl -si http://localhost:8001/kv/user:42 | awk '/^ETag:/ {print $2}')
curl -si http://localhost:8001/kv/user:42 -H "If-None-Match: $etag"
```

### Future Enhancements
- Use pub/sub messages to invalidate or refresh other replicas' caches (`LocalCache::erase`) using the key payload.
- Add TTLs or size limits to `LocalCache`.
- Input validation for POSTed JSON and optional schema enforcement.
- Structured error bodies and metrics.
