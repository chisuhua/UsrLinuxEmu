/*
 * test_kfd_module_standalone.cpp — C-12 B.1.11: kfd_module_init/exit 单元测试
 *
 * 测试范围（per tasks.md §B.1.11）:
 *   - init 返回 0
 *   - exit 是 no-op（void 返回）
 *   - init 幂等性（调用两次不崩溃，第二次返回 0）
 *   - exit 可配对调用
 *   - init → exit → init 循环（重新初始化正确）
 *
 * 链接:
 *   kfd_module.c, kfd_topology.c, kfd_svm.c, kfd_pasid.c,
 *   kfd_process.c, kfd_dispatch.c（全量 subsystem init）
 */
#include <catch_amalgamated.hpp>

extern "C" {
#include "kfd_module.h"
}

TEST_CASE("kfd_module init returns 0", "[kfd][module][init]") {
  int ret = kfd_module_init();
  REQUIRE(ret == 0);
  kfd_module_exit();
}

TEST_CASE("kfd_module exit is no-op", "[kfd][module][exit]") {
  /* exit before init should not crash */
  kfd_module_exit();
  SUCCEED("exit is safe before init");
}

TEST_CASE("kfd_module init is idempotent", "[kfd][module][idempotent]") {
  int ret1 = kfd_module_init();
  int ret2 = kfd_module_init();
  REQUIRE(ret1 == 0);
  REQUIRE(ret2 == 0);
  kfd_module_exit();
}

TEST_CASE("kfd_module init then exit cleanups", "[kfd][module][lifecycle]") {
  int ret = kfd_module_init();
  REQUIRE(ret == 0);
  kfd_module_exit();
  /* after exit, re-init should work */
  ret = kfd_module_init();
  REQUIRE(ret == 0);
  kfd_module_exit();
}

TEST_CASE("kfd_module re-init after full cycle", "[kfd][module][reinit]") {
  for (int i = 0; i < 3; i++) {
    int ret = kfd_module_init();
    REQUIRE(ret == 0);
    kfd_module_exit();
  }
  SUCCEED("init/exit cycle repeated 3 times without error");
}

TEST_CASE("kfd_module exit is idempotent", "[kfd][module][exit_idempotent]") {
  kfd_module_init();
  kfd_module_exit();
  kfd_module_exit();  /* double exit should not crash */
  SUCCEED("double exit is safe");
}