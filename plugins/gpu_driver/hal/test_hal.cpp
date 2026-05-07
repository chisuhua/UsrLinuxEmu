/*
 * test_hal.cpp — HAL 接口单元测试
 *
 * 验证 gpu_hal.h 接口定义：
 * - inline 包装函数正确调用函数指针
 * - ctx 指针正确传递
 * - 错误码返回路径
 * - 弹射式操作（void 返回）调用正常
 */
#include "gpu_hal.h"
#include "hal_mock.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static int tests_passed = 0;
static int tests_total = 0;

#define TEST(name) do { \
    tests_total++; \
    printf("  TEST: %s ... ", name); \
} while (0)

#define PASS() do { \
    tests_passed++; \
    printf("PASS\n"); \
} while (0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
    return 1; \
} while (0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) FAIL(msg); \
} while (0)

/* ── 测试 1: 基本函数指针调用 ──────────────────────── */

static int test_basic_calls() {
  TEST("register_read calls mock and returns value");

  struct gpu_hal_ops hal;
  struct hal_mock_state state;
  hal_mock_init(&hal, &state);

  state.register_read_out = 0x42;
  uint64_t val = 0;
  int ret = hal_register_read(&hal, 0x1000, &val);

  ASSERT(ret == 0, "expected success");
  ASSERT(val == 0x42, "expected 0x42");
  ASSERT(state.register_read_count == 1, "expected 1 call");
  PASS();
  return 0;
}

static int test_register_write() {
  TEST("register_write passes value through");

  struct gpu_hal_ops hal;
  struct hal_mock_state state;
  hal_mock_init(&hal, &state);

  hal_register_write(&hal, 0x2000, 0xDEAD);

  ASSERT(state.register_write_count == 1, "expected 1 call");
  ASSERT(state.last_reg_write_val == 0xDEAD, "expected 0xDEAD");
  PASS();
  return 0;
}

static int test_fire_and_forget() {
  TEST("void-return ops compile and count calls");

  struct gpu_hal_ops hal;
  struct hal_mock_state state;
  hal_mock_init(&hal, &state);

  hal_doorbell_ring(&hal, 3);
  hal_interrupt_raise(&hal, 7);
  hal_time_wait(&hal, 1000);

  ASSERT(state.doorbell_ring_count == 1, "doorbell count");
  ASSERT(state.interrupt_raise_count == 1, "interrupt count");
  ASSERT(state.time_wait_count == 1, "time_wait count");
  ASSERT(state.last_doorbell_queue == 3, "doorbell queue id");
  ASSERT(state.last_interrupt_vector == 7, "interrupt vector");
  ASSERT(state.last_time_wait_us == 1000, "time_wait us");
  PASS();
  return 0;
}

/* ── 测试 2: ctx 指针正确传递 ──────────────────────── */

static int test_ctx_passthrough() {
  TEST("ctx pointer reaches callback");

  struct gpu_hal_ops hal;
  struct hal_mock_state state;
  hal_mock_init(&hal, &state);

  /* 验证 ctx 被设置为 &state */
  ASSERT(hal.ctx == &state, "ctx should point to state");
  PASS();
  return 0;
}

/* ── 测试 3: 错误码传递 ────────────────────────────── */

static int test_error_return() {
  TEST("negative error codes pass through");

  struct gpu_hal_ops hal;
  struct hal_mock_state state;
  hal_mock_init(&hal, &state);

  state.register_read_result = -14;  /* -EFAULT */
  uint64_t val = 0;
  int ret = hal_register_read(&hal, 0, &val);
  ASSERT(ret == -14, "expected -EFAULT");

  state.mem_alloc_result = -12;  /* -ENOMEM */
  uint64_t addr = 0;
  ret = hal_mem_alloc(&hal, 4096, &addr);
  ASSERT(ret == -12, "expected -ENOMEM");

  PASS();
  return 0;
}

/* ── 测试 4: 内存操作路径 ──────────────────────────── */

static int test_mem_ops() {
  TEST("mem_read / mem_write / mem_alloc / mem_free roundtrip");

  struct gpu_hal_ops hal;
  struct hal_mock_state state;
  hal_mock_init(&hal, &state);

  /* 分配 */
  state.mem_alloc_out_addr = 0x100000;
  uint64_t addr = 0;
  int ret = hal_mem_alloc(&hal, 65536, &addr);
  ASSERT(ret == 0, "alloc success");
  ASSERT(addr == 0x100000, "alloc address");
  ASSERT(state.mem_alloc_count == 1, "alloc called");

  /* 写入 */
  char buf[64] = "hello gpu";
  ret = hal_mem_write(&hal, addr, buf, 10);
  ASSERT(ret == 0, "write success");
  ASSERT(state.last_mem_write_addr == 0x100000, "write address");

  /* 读取 */
  char out[64] = {};
  ret = hal_mem_read(&hal, addr, out, 10);
  ASSERT(ret == 0, "read success");

  /* 释放 */
  ret = hal_mem_free(&hal, addr);
  ASSERT(ret == 0, "free success");
  ASSERT(state.mem_free_count == 1, "free called");

  PASS();
  return 0;
}

/* ── 测试 5: fence 操作路径 ────────────────────────── */

static int test_fence_ops() {
  TEST("fence_create / fence_read roundtrip");

  struct gpu_hal_ops hal;
  struct hal_mock_state state;
  hal_mock_init(&hal, &state);

  state.fence_create_out_id = 42;
  uint64_t fence_id = 0;
  int ret = hal_fence_create(&hal, &fence_id);
  ASSERT(ret == 0, "fence_create success");
  ASSERT(fence_id == 42, "fence_id");
  ASSERT(state.fence_create_count == 1, "fence_create called");

  state.fence_read_out_val = 1;
  uint64_t val = 0;
  ret = hal_fence_read(&hal, fence_id, &val);
  ASSERT(ret == 0, "fence_read success");
  ASSERT(val == 1, "fence value");
  ASSERT(state.fence_read_count == 1, "fence_read called");

  PASS();
  return 0;
}

/* ── 测试 6: 错误注入路径 ──────────────────────────── */

static int test_inject_errors() {
  TEST("inject error on each int-returning op");

  struct gpu_hal_ops hal;
  struct hal_mock_state state;
  hal_mock_init(&hal, &state);

  state.register_read_result = -22;
  state.register_write_result = -22;
  state.mem_read_result = -14;
  state.mem_write_result = -14;
  state.mem_alloc_result = -12;
  state.mem_free_result = -5;   /* -EIO */
  state.fence_create_result = -22;
  state.fence_read_result = -22;

  uint64_t dummy = 0;
  ASSERT(hal_register_read(&hal, 0, &dummy) == -22, "reg_read err");
  ASSERT(hal_register_write(&hal, 0, 0) == -22, "reg_write err");
  ASSERT(hal_mem_read(&hal, 0, &dummy, 0) == -14, "mem_read err");
  ASSERT(hal_mem_write(&hal, 0, &dummy, 0) == -14, "mem_write err");
  ASSERT(hal_mem_alloc(&hal, 0, &dummy) == -12, "mem_alloc err");
  ASSERT(hal_mem_free(&hal, 0) == -5, "mem_free err");
  ASSERT(hal_fence_create(&hal, &dummy) == -22, "fence_create err");
  ASSERT(hal_fence_read(&hal, 0, &dummy) == -22, "fence_read err");

  PASS();
  return 0;
}

/* ── 测试 7-12: hal_user 实现测试 ──────────────────── */

#include "hal_user.h"

static int test_user_register_rw() {
  TEST("hal_user: register read/write roundtrip");

  struct gpu_hal_ops hal;
  struct hal_user_context uctx;
  hal_user_init(&hal, &uctx);

  hal_register_write(&hal, 0x80, 0xABCD1234);
  uint64_t val = 0;
  int ret = hal_register_read(&hal, 0x80, &val);
  ASSERT(ret == 0, "reg_read success");
  ASSERT(val == 0xABCD1234, "reg value matches");
  PASS();
  return 0;
}

static int test_user_mem_alloc_free() {
  TEST("hal_user: mem alloc/write/read/free roundtrip");

  struct gpu_hal_ops hal;
  struct hal_user_context uctx;
  hal_user_init(&hal, &uctx);

  uint64_t addr = 0;
  int ret = hal_mem_alloc(&hal, 65536, &addr);
  ASSERT(ret == 0, "alloc 64KB");
  ASSERT(addr >= 0x100000000ULL, "addr in VRAM range");

  const char *msg = "hello from HAL";
  ret = hal_mem_write(&hal, addr, msg, 14);
  ASSERT(ret == 0, "mem_write");

  char buf[32] = {};
  ret = hal_mem_read(&hal, addr, buf, 14);
  ASSERT(ret == 0, "mem_read");
  ASSERT(strcmp(buf, "hello from HAL") == 0, "data matches");

  ret = hal_mem_free(&hal, addr);
  ASSERT(ret == 0, "mem_free");
  PASS();
  return 0;
}

static int test_user_fence() {
  TEST("hal_user: fence create/read");

  struct gpu_hal_ops hal;
  struct hal_user_context uctx;
  hal_user_init(&hal, &uctx);

  uint64_t fid = 0;
  int ret = hal_fence_create(&hal, &fid);
  ASSERT(ret == 0, "fence_create");
  ASSERT(fid == 0, "first fence id=0");

  uint64_t val = 0;
  ret = hal_fence_read(&hal, fid, &val);
  ASSERT(ret == 0, "fence_read");
  ASSERT(val == 1, "fence signaled after create");
  PASS();
  return 0;
}

static int test_user_fire_and_forget() {
  TEST("hal_user: doorbell/interrupt/timewait");

  struct gpu_hal_ops hal;
  struct hal_user_context uctx;
  hal_user_init(&hal, &uctx);

  hal_doorbell_ring(&hal, 5);
  hal_interrupt_raise(&hal, 3);
  hal_time_wait(&hal, 1);  /* 1us */

  /* 无法验证时间，但确保不崩溃 */
  PASS();
  return 0;
}

static int test_user_mem_bad_params() {
  TEST("hal_user: bad params return -EINVAL");

  struct gpu_hal_ops hal;
  struct hal_user_context uctx;
  hal_user_init(&hal, &uctx);

  uint64_t val = 0;
  ASSERT(hal_register_read(&hal, 99999, &val) < 0, "bad reg offset");
  ASSERT(hal_mem_read(&hal, 0, &val, 999999) < 0, "bad mem size");
  PASS();
  return 0;
}

/* ── 主入口 ────────────────────────────────────────── */

int main() {
  printf("HAL 接口测试\n");
  printf("============\n");

  int rc = 0;
  rc |= test_basic_calls();
  rc |= test_register_write();
  rc |= test_fire_and_forget();
  rc |= test_ctx_passthrough();
  rc |= test_error_return();
  rc |= test_mem_ops();
  rc |= test_fence_ops();
  rc |= test_inject_errors();

  /* 用户态 HAL 实现测试 */
  rc |= test_user_register_rw();
  rc |= test_user_mem_alloc_free();
  rc |= test_user_fence();
  rc |= test_user_fire_and_forget();
  rc |= test_user_mem_bad_params();

  printf("\n结果: %d/%d 通过\n", tests_passed, tests_total);
  return rc;
}
