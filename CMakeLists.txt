cmake_minimum_required(VERSION 3.24)
project(NanoExchange VERSION 1.0.0 LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Werror)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-fsanitize=address,undefined") 

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
  googlebenchmark
  URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_library(nano_exchange_core INTERFACE)
target_include_directories(nano_exchange_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)


add_executable(nano_exchange_app src/main.cpp)
target_link_libraries(nano_exchange_app PRIVATE nano_exchange_core)

add_executable(bm_NaiveOrderBook benchmarks/bm_NaiveOrderBook.cpp)
target_link_libraries(bm_NaiveOrderBook PRIVATE nano_exchange_core benchmark::benchmark)

add_executable(bm_OptimizedOrderBook benchmarks/bm_OptimizedOrderBook.cpp)
target_link_libraries(bm_OptimizedOrderBook PRIVATE nano_exchange_core benchmark::benchmark)


add_executable(nano_exchange_tests tests/test_memory_pool.cpp tests/test_PriceLevel.cpp)
target_link_libraries(nano_exchange_tests PRIVATE nano_exchange_core gtest_main)

include(GoogleTest)
gtest_discover_tests(nano_exchange_tests)