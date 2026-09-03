// test_iommu_driver_standalone.cpp — Stage 5.5.1 Wave 1B（IOMMU driver 迁移验证）

#include <catch_amalgamated.hpp>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <type_traits>

#include <dlfcn.h>

#include "iommu_internal.h"

#include "test_plugin_loader_helper.h"

extern "C" {
#include <linux_compat/iommu/iommu.h>
}

namespace {

// 验证 plugins/iommu_driver/ 路径下 invalidate.cpp 真实存在
// （tasks.md 任务 2.6：invalidate.cpp 暂留 Q2）。
bool file_exists(const std::string& p) {
  return std::filesystem::exists(p);
}

}  // namespace

TEST_CASE("iommu_emu_init() 返回 0 且幂等",
          "[stage_5_5_1][iommu_driver][wave_1b]") {
  LOAD_PLUGINS_FOR_TESTS();
  REQUIRE(iommu_emu_init() == 0);
  // 幂等：第二次调用仍返回 0（iommu.cpp:36-37 检查 g->initialized）。
  REQUIRE(iommu_emu_init() == 0);
}

TEST_CASE("vfio_bridge 公共头文件存在 + us_iommu_vfio_available 返回 0",
          "[stage_5_5_1][iommu_driver][wave_1b]") {
  // tasks.md 任务 2.3：迁移 vfio_bridge.h/.cpp。
  // vfio_bridge.h 中的 us_iommu_vfio_available 在无 USR_LINUX_EMU_VFIO
  // 环境变量时应返回 0（vfio_bridge.cpp:37）。验证该契约而非实际加载。
  void* sym = dlsym(RTLD_DEFAULT, "us_iommu_vfio_available");
  REQUIRE(sym != nullptr);

  using vfio_avail_t = int (*)(void);
  auto fn = reinterpret_cast<vfio_avail_t>(sym);
  CHECK(fn() == 0);
}

TEST_CASE("iommu_internal.h 存在（usr_linux_emu::iommu_driver_global_state）",
          "[stage_5_5_1][iommu_driver][wave_1b]") {
  // tasks.md 任务 2.5：迁移 iommu_internal.h。
  // 编译期验证关键 C++ 符号可解析。
  using StatePtr = usr_linux_emu::iommu_emu_state*;
  StatePtr (*fn)(void) = &usr_linux_emu::iommu_driver_global_state;
  REQUIRE(fn != nullptr);
  REQUIRE(fn() != nullptr);
}

TEST_CASE("invalidate.cpp 暂留 Q2（plugins/iommu_driver/invalidate.cpp 存在）",
          "[stage_5_5_1][iommu_driver][wave_1b]") {
  // tasks.md 任务 2.6：v0.3 修订，invalidate.cpp 不迁入 sim_hardware。
  // 验证文件存在于 plugins/iommu_driver/ 而非 sim_hardware/。
  CHECK(file_exists("plugins/iommu_driver/invalidate.cpp"));

  // invalidate.cpp 导出的核心符号（dlopen 后可解析）。
  LOAD_PLUGINS_FOR_TESTS();
  void* sym = dlsym(RTLD_DEFAULT,
                    "iommu_invalidate_register_notifier_internal");
  CHECK(sym != nullptr);
}
