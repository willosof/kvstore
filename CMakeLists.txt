cmake_minimum_required(VERSION 3.22)
project(JsonKVService CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer legacy FindBoost over Boost's new config packages (fixes Homebrew Boost 1.89)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

# Homebrew OpenSSL on macOS
if(APPLE)
  set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
  set(OPENSSL_USE_STATIC_LIBS OFF)
endif()

set(Boost_NO_BOOST_CMAKE ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS OFF)
find_package(Boost 1.84 REQUIRED)
find_package(OpenSSL REQUIRED)

# Provide header-only aliases for Boost imported targets used by boost_redis when using FindBoost
if(Boost_FOUND)
  foreach(_boost_hdr IN ITEMS asio beast assert core mp11 describe system)
    if(NOT TARGET Boost::${_boost_hdr})
      add_library(Boost::${_boost_hdr} INTERFACE IMPORTED)
      target_include_directories(Boost::${_boost_hdr} INTERFACE ${Boost_INCLUDE_DIRS})
    endif()
  endforeach()
endif()

## Build Boost.Redis directly into the target via src.hpp (header-only integration)

add_executable(json_kv_service
  src/main.cpp
  src/cache.hpp
  src/data_layer.hpp
  src/data_layer.cpp
  src/http_handlers.hpp
  src/http_handlers.cpp
  src/boost_redis_src.cpp
)

if(Boost_FOUND)
  target_include_directories(json_kv_service PRIVATE ${Boost_INCLUDE_DIRS})
endif()

target_link_libraries(json_kv_service PRIVATE OpenSSL::SSL OpenSSL::Crypto)

target_compile_definitions(json_kv_service PRIVATE BOOST_ASIO_HAS_CO_AWAIT=1)


