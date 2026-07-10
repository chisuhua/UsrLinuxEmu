// SPDX-License-Identifier: MIT
// tests/perf/ioctl_dispatch_bench.cpp
//
// Benchmark: ioctl 派发延迟
//   - 测量 GPU_IOCTL_GET_DEVICE_INFO 单次调用 latency
//   - 1000 调用总耗时 + per-call mean / p99 / p999
//   - 用 Catch2 `BENCHMARK` macro 自动报告 ns/op
//
// 启动方式（项目根目录）：
//   cmake -DUSR_LINUX_EMU_PERF_TESTS=ON -B build && cmake --build build
//   ./build/bin/ioctl_dispatch_bench_standalone
//
// 测量对象：GpgpuDevice::ioctl 派发表（kTable），无 side-effect，仅 query。

#include <chrono>
#include <vector>
#include <algorithm>
#include <cstdio>

#include "perf_fixture.h"

using namespace usr_linux_emu::perf;
static PluginLifecycle g_plugin_lifecycle;

// 工具：计算 vector 的 mean / p99 / p999 (us)
struct Percentiles {
  double mean_us;
  double p50_us;
  double p99_us;
  double p999_us;
};

Percentiles compute_percentiles(std::vector<double>& samples_us) {
  std::sort(samples_us.begin(), samples_us.end());
  Percentiles p{};
  double sum = 0.0;
  for (auto v : samples_us) sum += v;
  p.mean_us = sum / samples_us.size();
  p.p50_us = samples_us[samples_us.size() / 2];
  p.p99_us = samples_us[(samples_us.size() * 99) / 100];
  p.p999_us = samples_us[(samples_us.size() * 999) / 1000];
  return p;
}

TEST_CASE("perf: ioctl dispatch latency (GET_DEVICE_INFO)",
          "[perf][ioctl][baseline]") {
  GpuPerfFixture fix;
  fix.warmup_ioctl(50);

  constexpr int kSamples = 1000;
  std::vector<double> samples_us;
  samples_us.reserve(kSamples);

  // Catch2 BENCHMARK macro：自动计算 ns/op + 通过 return 块控制 iters
  BENCHMARK("ioctl GET_DEVICE_INFO per-op") {
    return [&](int iters) {
      for (int i = 0; i < iters; ++i) {
        struct gpu_device_info info{};
        auto t0 = std::chrono::steady_clock::now();
        long r = fix.ioctl(GPU_IOCTL_GET_DEVICE_INFO, &info);
        auto t1 = std::chrono::steady_clock::now();
        REQUIRE(r == 0);
        // Only measure first iter (avoid benchmark overhead in samples)
        if (i == 0) {
          samples_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        }
      }
    };
  };

  // Persist 1000-call total & percentiles as a sanity report
  BENCHMARK("ioctl GET_DEVICE_INFO x1000") {
    return [&](int /*iters*/) {
      auto t0 = std::chrono::steady_clock::now();
      for (int i = 0; i < kSamples; ++i) {
        struct gpu_device_info info{};
        long r = fix.ioctl(GPU_IOCTL_GET_DEVICE_INFO, &info);
        REQUIRE(r == 0);
      }
      auto t1 = std::chrono::steady_clock::now();
      return std::chrono::duration<double, std::micro>(t1 - t0).count();
    };
  };

  // 末尾报告：手动统计 1000 个独立样本的 percentiles
  // (BENCHMARK macro 不直接给 percentiles，需要单独验证)
  // 这里用 Catch2 的标准 INFO/MESSAGE 输出 baseline 数据。
}

TEST_CASE("perf: ioctl dispatch latency report (1000-sample)",
          "[perf][ioctl][report]") {
  GpuPerfFixture fix;
  fix.warmup_ioctl(50);

  constexpr int kSamples = 1000;
  std::vector<double> samples_us;
  samples_us.reserve(kSamples);
  for (int i = 0; i < kSamples; ++i) {
    struct gpu_device_info info{};
    auto t0 = std::chrono::steady_clock::now();
    long r = fix.ioctl(GPU_IOCTL_GET_DEVICE_INFO, &info);
    auto t1 = std::chrono::steady_clock::now();
    REQUIRE(r == 0);
    samples_us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
  }
  Percentiles p = compute_percentiles(samples_us);
  std::printf("\n[perf][ioctl] GET_DEVICE_INFO x1000:\n");
  std::printf("  mean  = %.3f us\n", p.mean_us);
  std::printf("  p50   = %.3f us\n", p.p50_us);
  std::printf("  p99   = %.3f us\n", p.p99_us);
  std::printf("  p999  = %.3f us\n", p.p999_us);
}
