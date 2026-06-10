# Dependencies fetched from remote (GoogleTest + Google Benchmark).
#
# Included from the top-level CMakeLists.txt. Honors the ENABLE_TESTS /
# ENABLE_BENCHMARKS options to avoid network access when not needed.
#
# Pinned versions live here so upgrading a dependency is a one-line change.

if(ENABLE_TESTS OR ENABLE_BENCHMARKS)
    include(FetchContent)
endif()

if(ENABLE_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE
    )
    # Don't override this project's compiler/linker settings on Windows.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

if(ENABLE_BENCHMARKS)
    # Keep Google Benchmark from building its own tests / pulling in gtest.
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.9.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(benchmark)
endif()
