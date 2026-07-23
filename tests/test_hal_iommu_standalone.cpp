/*
 * test_hal_iommu_standalone.cpp — HAL IOMMU map/unmap 测试
 *
 * 测试 hal_user.cpp IOMMU operations 的完整功能，
 * 包括正常路径和错误路径。
 */
#include "catch_amalgamated.hpp"
#include "gpu_driver/hal/gpu_hal.h"
#include "gpu_driver/hal/hal_user.h"
#include <cstring>
#include <cerrno>

/* ─── Helper: 初始化 hal_user_context + gpu_hal_ops ─── */

struct IommuTestFixture {
  struct hal_user_context ctx{};
  struct gpu_hal_ops hal{};

  IommuTestFixture() {
    hal_user_init(&hal, &ctx);
  }
  ~IommuTestFixture() {
    hal_user_destroy(&ctx);
  }
};

/* ─── Test 1: iommu_map 正常路径 ─── */

TEST_CASE_METHOD(IommuTestFixture, "hal_user iommu_map returns success for valid mapping", "[hal_iommu][map]") {
  /* Map 4 pages (16 KB) at VA 0x1000 */
  int ret = hal_iommu_map(&hal, 0x1000, 4 * 4096, 0 /* domain_id */);
  REQUIRE(ret == 0);
}

/* ─── Test 2: iommu_unmap 正常路径 ─── */

TEST_CASE_METHOD(IommuTestFixture, "hal_user iommu_unmap returns success for mapped region", "[hal_iommu][unmap]") {
  /* First map, then unmap */
  int ret = hal_iommu_map(&hal, 0x2000, 2 * 4096, 0);
  REQUIRE(ret == 0);

  ret = hal_iommu_unmap(&hal, 0x2000, 2 * 4096);
  REQUIRE(ret == 0);
}

/* ─── Test 3: iommu_unmap unmapped region ─── */

TEST_CASE_METHOD(IommuTestFixture, "hal_user iommu_unmap returns error for never-mapped region", "[hal_iommu][unmap][error]") {
  /* Unmap a region that was never mapped */
  int ret = hal_iommu_unmap(&hal, 0xDEAD0000, 4096);
  REQUIRE(ret < 0);  /* Should return error (e.g. -EINVAL or -ENOENT) */
}

/* ─── Test 4: iommu_map invalid size ─── */

TEST_CASE_METHOD(IommuTestFixture, "hal_user iommu_map returns EINVAL for zero size", "[hal_iommu][map][error]") {
  int ret = hal_iommu_map(&hal, 0x1000, 0, 0);
  REQUIRE(ret == -EINVAL);
}

/* ─── Test 5: map/unmap/map cycle ─── */

TEST_CASE_METHOD(IommuTestFixture, "hal_user iommu_map/unmap/map cycle works correctly", "[hal_iommu][lifecycle]") {
  /* Map → unmap → remap same VA */
  int ret = hal_iommu_map(&hal, 0x4000, 4096, 0);
  REQUIRE(ret == 0);

  ret = hal_iommu_unmap(&hal, 0x4000, 4096);
  REQUIRE(ret == 0);

  /* Remap should succeed after unmap */
  ret = hal_iommu_map(&hal, 0x4000, 4096, 0);
  REQUIRE(ret == 0);
}

/* ─── Test 6: iommu_map double map should fail ─── */

TEST_CASE_METHOD(IommuTestFixture, "hal_user iommu_map returns error for double map", "[hal_iommu][map][error]") {
  int ret = hal_iommu_map(&hal, 0x5000, 4096, 0);
  REQUIRE(ret == 0);

  /* Double map same VA should fail */
  ret = hal_iommu_map(&hal, 0x5000, 4096, 0);
  REQUIRE(ret < 0);
}