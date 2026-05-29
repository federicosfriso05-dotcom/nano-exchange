# Create the benchmark executable
add_executable(bm_MemoryPool bm_MemoryPool.cpp)

# Link it to your code and the Google Benchmark libraries
target_include_directories(bm_MemoryPool PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bm_MemoryPool benchmark::benchmark benchmark::benchmark_main)

add_executable(bm_OptimizedOrderBook bm_OptimizedOrderBook.cpp)
target_link_libraries(bm_OptimizedOrderBook PRIVATE nano_exchange benchmark::benchmark benchmark::benchmark_main)

# Standalone latency microbenchmark (p50/p99 from per-iteration samples)
add_executable(lat_OptimizedOrderBook lat_OptimizedOrderBook.cpp)
target_link_libraries(lat_OptimizedOrderBook PRIVATE nano_exchange)