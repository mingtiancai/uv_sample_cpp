#include <benchmark/benchmark.h>

#include <uv.h>

#include <cstring>

// Placeholder benchmark demonstrating the harness. Replace with real
// performance-critical paths as the project grows.

namespace {

void BM_Ip4AddrParse(benchmark::State& state) {
  sockaddr_in addr;
  for (auto _ : state) {
    std::memset(&addr, 0, sizeof(addr));
    uv_ip4_addr("127.0.0.1", 9123, &addr);
    benchmark::DoNotOptimize(addr);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(BM_Ip4AddrParse);

}  // namespace
