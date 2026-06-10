# Unit tests (GoogleTest).
#
# Each test executable links against GTest::gtest_main and is registered with
# CTest via gtest_discover_tests so `ctest` picks up every TEST() case.

add_executable(example_test
    example_test.cpp
)

target_link_libraries(example_test PRIVATE
    GTest::gtest_main
    libuv::uv
)

# Auto-register all TEST()/TEST_F() cases with CTest.
include(GoogleTest)
gtest_discover_tests(example_test)
