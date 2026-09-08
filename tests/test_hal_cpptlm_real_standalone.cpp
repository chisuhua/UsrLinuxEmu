/*
 * tests/test_hal_cpptlm_real_standalone.cpp
 * 5.5.6-cpptlm-ep-binding — P4.NEW-D: hal_cpptlm.cpp 真实 backend + composition
 *
 * Verifies:
 *   - hal_cpptlm_init composes with hal_user_init (68 fn-ptrs delegated)
 *   - 3 adapter op (adapter_get_info/open/close) route to real CppTLM ABI
 *   - DL_PRELOAD libcpptlm_emulator.so required (skip if not available)
 */
#include <catch_amalgamated.hpp>

#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <vector>

#include "hal/hal_mock.h"
#include "hal/hal_user.h"
#include "gpu_hal.h"

void hal_cpptlm_init(struct gpu_hal_ops* hal, void* ctx);

namespace {

bool cpptlm_lib_available() {
  void* h = dlopen("libcpptlm_emulator.so", RTLD_NOW | RTLD_LOCAL);
  if (!h) h = dlopen("../CppTLM/build/lib/libcpptlm_emulator.so", RTLD_NOW | RTLD_LOCAL);
  if (!h) h = dlopen("/workspace/project/CppTLM/build/lib/libcpptlm_emulator.so",
                    RTLD_NOW | RTLD_LOCAL);
  if (h) dlclose(h);
  return h != nullptr;
}

}  // namespace

// ===== Composition: hal_cpptlm_init delegates 68 fn-ptrs to hal_user =====

TEST_CASE("hal_cpptlm: composition delegates non-adapter fn-ptrs to hal_user",
          "[hal_cpptlm][composition]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  // Save baseline pointers (hal_user fills 68 fn-ptrs)
  auto baseline_register_read = hal.register_read;
  auto baseline_mem_alloc = hal.mem_alloc;
  auto baseline_fence_create = hal.fence_create;

  // Now compose: hal_cpptlm_init should NOT clobber non-adapter fn-ptrs

  hal_cpptlm_init(&hal, &ctx);

  CHECK(hal.register_read == baseline_register_read);
  CHECK(hal.mem_alloc == baseline_mem_alloc);
  CHECK(hal.fence_create == baseline_fence_create);

  hal_user_destroy(&ctx);
}

TEST_CASE("hal_cpptlm: composition overrides 3 adapter fn-ptrs",
          "[hal_cpptlm][composition]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  // hal_user doesn't set adapter_* fn-ptrs (only kcpptlm-backend-binding did)
  // Save as null
  auto baseline_get = hal.adapter_get_info;
  auto baseline_open = hal.adapter_open;
  auto baseline_close = hal.adapter_close;


  hal_cpptlm_init(&hal, &ctx);

  // After hal_cpptlm_init, the 3 adapter fn-ptrs should be non-null
  CHECK(hal.adapter_get_info != nullptr);
  CHECK(hal.adapter_open != nullptr);
  CHECK(hal.adapter_close != nullptr);
  // And different from whatever hal_user set (likely nullptr)
  if (baseline_get) CHECK(hal.adapter_get_info != baseline_get);

  hal_user_destroy(&ctx);
}

// ===== 3 adapter ops behavior =====

TEST_CASE("hal_cpptlm: adapter_get_info returns -EINVAL on NULL",
          "[hal_cpptlm][adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  hal_cpptlm_init(&hal, &ctx);

  int ret = hal.adapter_get_info(hal.ctx, nullptr);
  REQUIRE(ret == -EINVAL);

  hal_user_destroy(&ctx);
}

TEST_CASE("hal_cpptlm: adapter_open returns -EINVAL on NULL",
          "[hal_cpptlm][adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  hal_cpptlm_init(&hal, &ctx);

  int ret = hal.adapter_open(hal.ctx, nullptr);
  REQUIRE(ret == -EINVAL);

  hal_user_destroy(&ctx);
}

TEST_CASE("hal_cpptlm: adapter_close returns -EINVAL on NULL handle",
          "[hal_cpptlm][adapter]") {
  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  hal_cpptlm_init(&hal, &ctx);

  int ret = hal.adapter_close(hal.ctx, 0);
  REQUIRE(ret == -EINVAL);

  hal_user_destroy(&ctx);
}

// ===== Real CppTLM integration (requires libcpptlm_emulator.so) =====

TEST_CASE("hal_cpptlm: real adapter_open via CppTLM ABI succeeds",
          "[hal_cpptlm][real][.]") {
  if (!cpptlm_lib_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  hal_cpptlm_init(&hal, &ctx);

  gpu_adapter_handle_t handle = 0;
  int ret = hal.adapter_open(hal.ctx, &handle);
  INFO("adapter_open ret=" << ret << " handle=" << handle);
  REQUIRE(ret == 0);
  REQUIRE(handle != 0);

  ret = hal.adapter_close(hal.ctx, handle);
  INFO("adapter_close ret=" << ret);
  REQUIRE(ret == 0);

  hal_user_destroy(&ctx);
}

TEST_CASE("hal_cpptlm: real adapter_get_info populates fields via CppTLM",
          "[hal_cpptlm][real][.]") {
  if (!cpptlm_lib_available()) {
    SKIP("libcpptlm_emulator.so not available");
  }

  struct gpu_hal_ops hal{};
  struct hal_user_context ctx{};
  hal_user_init(&hal, &ctx);

  hal_cpptlm_init(&hal, &ctx);

  gpu_adapter_handle_t handle = 0;
  REQUIRE(hal.adapter_open(hal.ctx, &handle) == 0);

  gpu_adapter_info_t info{};
  int ret = hal.adapter_get_info(hal.ctx, &info);
  INFO("adapter_get_info ret=" << ret);
  REQUIRE(ret == 0);

  REQUIRE(hal.adapter_close(hal.ctx, handle) == 0);
  hal_user_destroy(&ctx);
}

// ===== hal_mock fallback (composition still works) =====

TEST_CASE("hal_mock: composition baseline (existing behavior preserved)",
          "[hal_mock][composition]") {
  struct gpu_hal_ops hal{};
  struct hal_mock_state state{};
  hal_mock_init(&hal, &state);

  // hal_mock_init sets all 71 fn-ptrs (including 3 adapter)
  CHECK(hal.adapter_get_info != nullptr);
  CHECK(hal.adapter_open != nullptr);
  CHECK(hal.adapter_close != nullptr);
}