# syntax=docker/dockerfile:1.7

FROM --platform=$BUILDPLATFORM alpine:3.20 AS build
ARG TARGETPLATFORM
ARG BUILDPLATFORM

RUN apk add --no-cache build-base cmake git openssl-dev boost-dev

WORKDIR /src
COPY . /src

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j

FROM alpine:3.20
RUN apk add --no-cache openssl boost-system boost-thread
WORKDIR /app
COPY --from=build /src/build/json_kv_service /app/json_kv_service

ENV PORT=8001 \
    BIND_HOST=0.0.0.0 \
    KV_API_KEY=change_me \
    REDIS_HOST=redis \
    REDIS_PORT=6379

EXPOSE 8001
ENTRYPOINT ["/app/json_kv_service"]
CMD ["8001"]


