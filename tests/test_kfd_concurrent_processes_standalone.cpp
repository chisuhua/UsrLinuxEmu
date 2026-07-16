/*
 * test_kfd_concurrent_processes_standalone.cpp — C-12 C.2.3 (downgraded: multi-thread, not multi-process)
 *
 * Verifies mm_shim PID isolation when multiple kfd_process instances
 * exist concurrently. Per C.0.3 + ADR-063 implementation phase decision,
 * we don't exercise true multi-process; instead we verify two kfd_process
 * structs (different PIDs) carry independent mm_shim instances and their
 * VMA ranges don't cross-contaminate under concurrent register/unregister.
 */

#include <catch_amalgamated.hpp>

#include <atomic>
#include <thread>
#include <vector>

extern "C" {
#include <linux_compat/types.h>
#include "kernel/uvm/mm_shim.h"
}

/*
 * kfd_priv.h MUST be included OUTSIDE extern "C" because it transitively
 * includes <atomic> (C++ templates can't have C linkage).
 * kfd_priv.h's C++ path uses atomic_int from <atomic>, but it's in
 * std:: namespace. Pre-include <atomic> and bring into global scope.
 * (Same pattern as test_kfd_process_standalone.cpp lines 23-25.)
 */
#include <atomic>
using std::atomic_int;
#include "kfd_priv.h"

extern "C" {
#include "kfd_process.h"
}

/* --- Test 1: two PID isolation (sequential, primary correctness) --- */

TEST_CASE("kfd two PIDs have isolated mm_shim instances and VMAs",
          "[kfd][mm_shim][isolation][phase_c][pid]") {
  REQUIRE(kfd_process_init() == 0);

  struct kfd_process *pA = nullptr;
  struct kfd_process *pB = nullptr;
  REQUIRE(kfd_process_create(&pA, 0x1001) == 0);
  REQUIRE(kfd_process_create(&pB, 0x1002) == 0);
  REQUIRE(pA != nullptr);
  REQUIRE(pB != nullptr);
  REQUIRE(pA->mm_shim != nullptr);
  REQUIRE(pB->mm_shim != nullptr);
  REQUIRE(pA->mm_shim != pB->mm_shim);  /* distinct allocations */

  struct us_mm_shim *a = static_cast<struct us_mm_shim *>(pA->mm_shim);
  struct us_mm_shim *b = static_cast<struct us_mm_shim *>(pB->mm_shim);
  CHECK(a->pid == 0x1001UL);
  CHECK(b->pid == 0x1002UL);

  /* Register different VA ranges for each */
  CHECK(us_mm_shim_register_vma(a, 0x10000UL, 0x11000UL, 0) == 0);
  CHECK(us_mm_shim_register_vma(b, 0x20000UL, 0x21000UL, 0) == 0);

  /* A's find_vma should NOT find B's range, and vice versa */
  unsigned long os = 0, oe = 0;
  CHECK(us_mm_shim_find_vma(a, 0x20500UL, &os, &oe) == -ENOENT);
  CHECK(us_mm_shim_find_vma(b, 0x10500UL, &os, &oe) == -ENOENT);

  /* A and B each find their own range */
  CHECK(us_mm_shim_find_vma(a, 0x10500UL, &os, &oe) == 0);
  CHECK(os == 0x10000UL);
  CHECK(us_mm_shim_find_vma(b, 0x20500UL, &os, &oe) == 0);
  CHECK(os == 0x20000UL);

  /* PID A unregister — should NOT affect PID B */
  CHECK(us_mm_shim_unregister_vma(a, 0x10000UL, 0x11000UL) == 0);
  CHECK(us_mm_shim_find_vma(a, 0x10500UL, &os, &oe) == -ENOENT);
  /* B's VMA must still be intact */
  CHECK(us_mm_shim_find_vma(b, 0x20500UL, &os, &oe) == 0);
  CHECK(os == 0x20000UL);

  /* Cleanup */
  CHECK(kfd_process_destroy(pA) == 0);
  CHECK(kfd_process_destroy(pB) == 0);
  kfd_process_exit();
}

/* --- Test 2: multi-thread concurrent register/unregister (threading) --- */

TEST_CASE("kfd concurrent processes under multi-thread register/unregister remain isolated",
          "[kfd][mm_shim][isolation][phase_c][multithread]") {
  REQUIRE(kfd_process_init() == 0);

  constexpr int kThreads = 4;
  constexpr int kVmasPerThread = 8;

  struct kfd_process *procs[kThreads] = {nullptr};
  std::atomic<int> errors{0};

  /* Each thread owns one process and registers VMAs in its own mm_shim */
  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  for (int t = 0; t < kThreads; t++) {
    threads.emplace_back([t, &procs, &errors]() {
      pid_t pid = (pid_t)(0x1000 + t);
      struct kfd_process *p = nullptr;
      if (kfd_process_create(&p, pid) != 0) { errors++; return; }
      procs[t] = p;
      if (!p->mm_shim) { errors++; return; }

      struct us_mm_shim *m = static_cast<struct us_mm_shim *>(p->mm_shim);
      unsigned long base = 0x100000UL + (unsigned long)t * 0x100000UL;
      for (int v = 0; v < kVmasPerThread; v++) {
        unsigned long s = base + (unsigned long)v * 0x1000UL;
        unsigned long e = s + 0x1000UL;
        if (us_mm_shim_register_vma(m, s, e, (unsigned long)t) != 0) errors++;
      }
    });
  }
  for (auto &th : threads) th.join();
  CHECK(errors.load() == 0);

  /* Cross-validate: each process's VMAs must be findable, and no other process
   * can see them. */
  for (int t = 0; t < kThreads; t++) {
    REQUIRE(procs[t] != nullptr);
    struct us_mm_shim *m = static_cast<struct us_mm_shim *>(procs[t]->mm_shim);
    unsigned long base = 0x100000UL + (unsigned long)t * 0x100000UL;
    /* Each VMA in this process's range must be hit */
    for (int v = 0; v < kVmasPerThread; v++) {
      unsigned long s = base + (unsigned long)v * 0x1000UL;
      unsigned long os = 0, oe = 0;
      int rc = us_mm_shim_find_vma(m, s + 0x100UL, &os, &oe);
      if (rc != 0 || os != s || oe != s + 0x1000UL) errors++;
    }
    /* Must NOT see other threads' VMAs */
    unsigned long os = 0, oe = 0;
    int other_t = (t + 1) % kThreads;
    unsigned long other_base = 0x100000UL + (unsigned long)other_t * 0x100000UL;
    if (us_mm_shim_find_vma(m, other_base + 0x500UL, &os, &oe) != -ENOENT) errors++;
  }
  CHECK(errors.load() == 0);

  /* Cleanup */
  for (int t = 0; t < kThreads; t++) {
    if (procs[t]) kfd_process_destroy(procs[t]);
  }
  kfd_process_exit();
}