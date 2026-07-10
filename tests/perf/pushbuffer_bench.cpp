// SPDX-License-Identifier: MIT
// tests/perf/pushbuffer_bench.cpp
//
// Benchmark: GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH 延迟与吞吐
//   - 单独 submit 延迟（warmup + measured，含 fence 创建）
//   - 100 submits/sec 持续吞吐测量（steady-state，单 entry 模式）
//
// 实测对象：GpgpuDevice::ioctl → handlePushbufferSubmitBatch → HAL.queue_submit
//           → GpuQueueEmu::submit → enqueue to Scheduler → drain by sim Puller。

#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include <cstdio>

#include "perf_fixture.h"

using namespace usr_linux_emu::perf;
static PluginLifecycle g_plugin_lifecycle;

namespace {

struct PushbufferSetup {
  GpuPerfFixture* fix;
  uint64_t va_space_handle;
  uint64_t queue_handle;

  PushbufferSetup(GpuPerfFixture* f) : fix(f), va_space_handle(0), queue_handle(0) {
    struct gpu_va_space_args va_args{};
    va_args.page_size = 0;
    REQUIRE(fix->ioctl(GPU_IOCTL_CREATE_VA_SPACE, &va_args) == 0);
    va_space_handle = va_args.va_space_handle;

    struct gpu_queue_args q_args{};
    q_args.va_space_handle = va_space_handle;
    q_args.queue_type = 0;
    q_args.priority = 0;
    q_args.ring_buffer_size = 64 * sizeof(gpu_gpfifo_entry);
    REQUIRE(fix->ioctl(GPU_IOCTL_CREATE_QUEUE, &q_args) == 0);
    queue_handle = static_cast<uint64_t>(q_args.queue_handle);
  }
};

void make_single_entry(gpu_gpfifo_entry* e) {
  std::memset(e, 0, sizeof(*e));
  e->valid = 1;
  e->method = GPU_OP_LAUNCH_KERNEL;
  e->payload[0] = 0;  // kernel_idx
  e->payload[1] = 1;  // grid_dim
  e->payload[2] = 1;  // block_dim
}

}  // namespace

TEST_CASE("perf: pushbuffer submit latency (single)",
          "[perf][pushbuffer][baseline]") {
  GpuPerfFixture fix;
  PushbufferSetup setup(&fix);

  constexpr int kEntries = 1;
  gpu_gpfifo_entry entries[kEntries];
  make_single_entry(&entries[0]);

  for (int i = 0; i < 5; ++i) {
    struct gpu_pushbuffer_args pb{};
    pb.stream_id = setup.queue_handle;
    pb.va_space_handle = setup.va_space_handle;
    pb.entries_addr = reinterpret_cast<uintptr_t>(entries);
    pb.count = kEntries;
    long r = fix.ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);
    REQUIRE(r == 0);
  }

  BENCHMARK("pushbuffer submit single (1 entry)") {
    return [&](int iters) {
      for (int i = 0; i < iters; ++i) {
        struct gpu_pushbuffer_args pb{};
        pb.stream_id = setup.queue_handle;
        pb.va_space_handle = setup.va_space_handle;
        pb.entries_addr = reinterpret_cast<uintptr_t>(entries);
        pb.count = kEntries;
        long r = fix.ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);
        REQUIRE(r == 0);
      }
    };
  };
}

TEST_CASE("perf: pushbuffer throughput (100 submits/sec, single-entry)",
          "[perf][pushbuffer][throughput]") {
  GpuPerfFixture fix;
  PushbufferSetup setup(&fix);

  constexpr int kEntries = 1;
  gpu_gpfifo_entry entries[kEntries];
  make_single_entry(&entries[0]);

  constexpr int kTargetRate = 100;
  constexpr int kDurationSec = 2;
  constexpr int kTargetTotal = kTargetRate * kDurationSec;

  int success_count = 0;
  int fail_count = 0;
  std::vector<int> fail_ret_codes;

  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < kTargetTotal; ++i) {
    struct gpu_pushbuffer_args pb{};
    pb.stream_id = setup.queue_handle;
    pb.va_space_handle = setup.va_space_handle;
    pb.entries_addr = reinterpret_cast<uintptr_t>(entries);
    pb.count = kEntries;
    long r = fix.ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);
    if (r == 0) {
      success_count++;
    } else {
      fail_count++;
      fail_ret_codes.push_back(static_cast<int>(r));
    }

    auto next = t0 + std::chrono::milliseconds((i + 1) * 1000 / kTargetRate);
    std::this_thread::sleep_until(next);
  }
  auto t1 = std::chrono::steady_clock::now();
  double elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
  double actual_rate = success_count / elapsed_sec;
  std::printf("\n[perf][pushbuffer] 100-submits/sec sustained over %d sec:\n",
              kDurationSec);
  std::printf("  target rate = %d submits/sec\n", kTargetRate);
  std::printf("  success     = %d (%d failed)\n", success_count, fail_count);
  std::printf("  actual rate = %.1f submits/sec\n", actual_rate);
  std::printf("  elapsed     = %.3f sec\n", elapsed_sec);
  if (fail_count > 0) {
    std::printf("  fail codes  = first 5: ");
    for (size_t i = 0; i < std::min<size_t>(5, fail_ret_codes.size()); ++i) {
      std::printf("%d ", fail_ret_codes[i]);
    }
    std::printf("\n");
  }
  REQUIRE(actual_rate >= kTargetRate * 0.6);
}

TEST_CASE("perf: pushbuffer max throughput (no rate-limit)",
          "[perf][pushbuffer][max-throughput]") {
  GpuPerfFixture fix;
  PushbufferSetup setup(&fix);

  constexpr int kEntries = 1;
  gpu_gpfifo_entry entries[kEntries];
  make_single_entry(&entries[0]);

  constexpr int kWarmup = 5;
  constexpr int kMeasured = 1000;

  for (int i = 0; i < kWarmup; ++i) {
    struct gpu_pushbuffer_args pb{};
    pb.stream_id = setup.queue_handle;
    pb.va_space_handle = setup.va_space_handle;
    pb.entries_addr = reinterpret_cast<uintptr_t>(entries);
    pb.count = kEntries;
    long r = fix.ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);
    REQUIRE(r == 0);
  }

  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < kMeasured; ++i) {
    struct gpu_pushbuffer_args pb{};
    pb.stream_id = setup.queue_handle;
    pb.va_space_handle = setup.va_space_handle;
    pb.entries_addr = reinterpret_cast<uintptr_t>(entries);
    pb.count = kEntries;
    long r = fix.ioctl(GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH, &pb);
    REQUIRE(r == 0);
  }
  auto t1 = std::chrono::steady_clock::now();
  double elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
  double actual_rate = kMeasured / elapsed_sec;
  std::printf("\n[perf][pushbuffer] max-throughput (no sleep):\n");
  std::printf("  measured    = %d submits\n", kMeasured);
  std::printf("  elapsed     = %.3f sec\n", elapsed_sec);
  std::printf("  actual rate = %.1f submits/sec\n", actual_rate);
}
