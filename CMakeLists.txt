cmake_minimum_required(VERSION 3.24)
project(NanoExchange VERSION 1.0.0 LANGUAGES CXX)
enable_testing()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Disattiva estensioni proprietarie, vogliamo C++ puro


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")


set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer")


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


include_directories(include)

# (Queste righe le sbloccheremo quando creeremo i file veri e propri dentro queste cartelle)
# add_subdirectory(src)
# Create the test executable with both files
add_executable(nano_exchange_tests tests/test_memory_pool.cpp tests/test_PriceLevel.cpp)

# Link Google Test to our executable so it understands the TEST() macros
target_link_libraries(nano_exchange_tests gtest_main)

# Tell CMake to automatically find and register the tests we wrote
include(GoogleTest)
gtest_discover_tests(nano_exchange_tests)