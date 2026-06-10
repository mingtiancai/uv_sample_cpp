# Performance benchmarks (Google Benchmark).
#
# Run a benchmark binary directly, e.g.:
#   ./build/benchmarks/example_benchmark
#   ./build/benchmarks/example_benchmark --benchmark_filter=Ip4Addr

add_executable(example_benchmark
    example_benchmark.cpp
)

target_link_libraries(example_benchmark PRIVATE
    benchmark::benchmark_main
    libuv::uv
)
