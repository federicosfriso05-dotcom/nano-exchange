# Create the test executable
add_executable(test_memory_pool test_memory_pool.cpp)

# Link it against our include directory and the Google Test main library
target_include_directories(test_memory_pool PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_memory_pool GTest::gtest_main)

# Tell CMake this is a test so it shows up when we run the 'ctest' command
include(GoogleTest)
gtest_discover_tests(test_memory_pool)