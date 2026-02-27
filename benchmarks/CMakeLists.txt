# Create the benchmark executable
add_executable(bm_MemoryPool bm_MemoryPool.cpp)

# Link it to your code and the Google Benchmark libraries
target_include_directories(bm_MemoryPool PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(bm_MemoryPool benchmark::benchmark benchmark::benchmark_main)