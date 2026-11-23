cmake_minimum_required(VERSION 3.20)
project(RelayForge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings and optimization flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O2 -pthread)
endif()

# Find system libraries
find_package(SQLite3 REQUIRED)
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SQLite3_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
)

# Core library
set(RELAYFORGE_CORE_SOURCES
    src/types.cpp
    src/sqlite_storage.cpp
    src/validator.cpp
    src/rate_limiter.cpp
    src/priority_scheduler.cpp
    src/event_handlers.cpp
    src/retry_manager.cpp
    src/worker_pool.cpp
    src/metrics_registry.cpp
    src/recovery_manager.cpp
    src/engine.cpp
)

add_library(relayforge_core STATIC
    ${RELAYFORGE_CORE_SOURCES}
)

target_link_libraries(relayforge_core PUBLIC
    SQLite::SQLite3
    Threads::Threads
)

# Server executable
add_executable(relayforge_server
    src/server_main.cpp
)

target_link_libraries(relayforge_server PRIVATE
    relayforge_core
    cpp-httplib
    Threads::Threads
)

# Benchmark executable
add_executable(relayforge_benchmark
    benchmarks/benchmark_engine.cpp
)

target_link_libraries(relayforge_benchmark PRIVATE
    relayforge_core
    Threads::Threads
)

# Enable testing
enable_testing()
add_subdirectory(tests)
