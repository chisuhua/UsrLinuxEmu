/*
 * test_kfd_process_standalone.cpp — C-12 B.1.5 process management tests
 *
 * Verifies the kfd_process.h public API:
 *   - init/exit lifecycle (idempotency)
 *   - create/destroy with pasid allocation
 *   - duplicate pid rejection (EBUSY)
 *   - find_by_pid lookup
 *   - process count tracking
 *   - gpuid_from_node single-GPU mapping
 *   - null/invalid input error handling
 *
 * ISOLATED tests: do NOT include kfd_queue.c or test through queue path.
 */
#include <catch_amalgamated.hpp>

/*
 * kfd_priv.h MUST be included OUTSIDE extern "C" because it transitively
 * includes <atomic> (C++ templates can't have C linkage).
 * kfd_priv.h's C++ path uses atomic_int from <atomic>, but it's in
 * std:: namespace. Pre-include <atomic> and bring into global scope.
 */
#include <atomic>
using std::atomic_int;
#include "kfd_priv.h"

extern "C" {
#include "kfd_process.h"
#include "kfd_pasid.h"
}

/* mm_shim.h has extern "C" guards → safe to include outside the block */
#include <kernel/uvm/mm_shim.h>

/* --- lifecycle --- */

TEST_CASE("kfd_process init/exit", "[kfd][process]") {
  /* idempotent init */
  REQUIRE(kfd_process_init() == 0);
  REQUIRE(kfd_process_init() == 0);
  REQUIRE(kfd_process_init() == 0);

  /* exit cleans up */
  kfd_process_exit();

  /* re-init after exit */
  REQUIRE(kfd_process_init() == 0);
  kfd_process_exit();
}

TEST_CASE("kfd_process create/destroy", "[kfd][process]") {
  REQUIRE(kfd_process_init() == 0);

  struct kfd_process *p = NULL;
  REQUIRE(kfd_process_create(&p, 1000) == 0);
  REQUIRE(p != NULL);
  REQUIRE(p->pid == 1000);
  REQUIRE(p->pasid > 0);
  REQUIRE(p->mm == NULL);
  REQUIRE(p->lead_thread == NULL);
  REQUIRE(p->n_pdds == 0);

  REQUIRE(kfd_process_destroy(p) == 0);
  kfd_process_exit();
}

TEST_CASE("kfd_process create with same pid returns EBUSY", "[kfd][process]") {
  REQUIRE(kfd_process_init() == 0);

  struct kfd_process *p1 = NULL, *p2 = NULL;
  REQUIRE(kfd_process_create(&p1, 2000) == 0);
  REQUIRE(p1 != NULL);

  int ret = kfd_process_create(&p2, 2000);
  REQUIRE(ret == -16); /* -EBUSY */
  REQUIRE(p2 == NULL);

  REQUIRE(kfd_process_destroy(p1) == 0);
  kfd_process_exit();
}

/* --- lookup --- */

TEST_CASE("kfd_process find_by_pid success", "[kfd][process]") {
  REQUIRE(kfd_process_init() == 0);

  struct kfd_process *p = NULL;
  REQUIRE(kfd_process_create(&p, 3000) == 0);

  struct kfd_process *found = NULL;
  REQUIRE(kfd_process_find_by_pid(3000, &found) == 0);
  REQUIRE(found == p);

  REQUIRE(kfd_process_destroy(p) == 0);
  kfd_process_exit();
}

TEST_CASE("kfd_process find_by_pid missing returns ENOENT", "[kfd][process]") {
  REQUIRE(kfd_process_init() == 0);

  struct kfd_process *found = NULL;
  int ret = kfd_process_find_by_pid(9999, &found);
  REQUIRE(ret == -2); /* -ENOENT */
  REQUIRE(found == NULL);

  kfd_process_exit();
}

/* --- count --- */

TEST_CASE("kfd_process count tracks creates and destroys", "[kfd][process]") {
  REQUIRE(kfd_process_init() == 0);
  REQUIRE(kfd_process_count() == 0);

  struct kfd_process *a = NULL, *b = NULL, *c = NULL;
  REQUIRE(kfd_process_create(&a, 101) == 0);
  REQUIRE(kfd_process_count() == 1);

  REQUIRE(kfd_process_create(&b, 102) == 0);
  REQUIRE(kfd_process_count() == 2);

  REQUIRE(kfd_process_create(&c, 103) == 0);
  REQUIRE(kfd_process_count() == 3);

  REQUIRE(kfd_process_destroy(b) == 0);
  REQUIRE(kfd_process_count() == 2);

  REQUIRE(kfd_process_destroy(c) == 0);
  REQUIRE(kfd_process_count() == 1);

  REQUIRE(kfd_process_destroy(a) == 0);
  REQUIRE(kfd_process_count() == 0);

  kfd_process_exit();
}

/* --- gpuid_from_node --- */

TEST_CASE("kfd_process_gpuid_from_node single-GPU returns 0/0", "[kfd][process]") {
  REQUIRE(kfd_process_init() == 0);

  struct kfd_process *p = NULL;
  REQUIRE(kfd_process_create(&p, 4000) == 0);

  struct kfd_node dev;
  dev.id = 0;
  dev.xcc_mask = 0;

  struct kfd_process_device pdd;
  pdd.dev = &dev;
  pdd.process = p;
  pdd.drm_priv = NULL;

  p->pdds[0] = &pdd;
  p->n_pdds = 1;

  u32 gpuid = 99, gpuidx = 99;
  REQUIRE(kfd_process_gpuid_from_node(p, &dev, &gpuid, &gpuidx) == 0);
  REQUIRE(gpuid == 0);
  REQUIRE(gpuidx == 0);

  REQUIRE(kfd_process_destroy(p) == 0);
  kfd_process_exit();
}

TEST_CASE("kfd_process_gpuid_from_node unattached returns ENOENT", "[kfd][process]") {
  REQUIRE(kfd_process_init() == 0);

  struct kfd_process *p = NULL;
  REQUIRE(kfd_process_create(&p, 5000) == 0);

  struct kfd_node dev;
  dev.id = 1;
  dev.xcc_mask = 0;

  u32 gpuid = 99, gpuidx = 99;
  int ret = kfd_process_gpuid_from_node(p, &dev, &gpuid, &gpuidx);
  REQUIRE(ret == -2); /* -ENOENT */
  REQUIRE(gpuid == 99);  /* unchanged */
  REQUIRE(gpuidx == 99); /* unchanged */

  REQUIRE(kfd_process_destroy(p) == 0);
  kfd_process_exit();
}

/* --- error handling --- */

TEST_CASE("kfd_process destroy null returns EINVAL", "[kfd][process]") {
  REQUIRE(kfd_process_init() == 0);
  REQUIRE(kfd_process_destroy(NULL) == -22); /* -EINVAL */
  kfd_process_exit();
}

TEST_CASE("kfd_process create null out returns EINVAL", "[kfd][process]") {
  REQUIRE(kfd_process_init() == 0);
  REQUIRE(kfd_process_create(NULL, 42) == -22); /* -EINVAL */
  kfd_process_exit();
}

/* --- exit destroys all live processes --- */

TEST_CASE("kfd_process exit destroys remaining processes", "[kfd][process]") {
  REQUIRE(kfd_process_init() == 0);

  struct kfd_process *p = NULL;
  REQUIRE(kfd_process_create(&p, 6000) == 0);
  REQUIRE(kfd_process_count() == 1);

  /* exit should destroy p */
  kfd_process_exit();

  /* after exit, registry is empty and re-initialized */
  REQUIRE(kfd_process_count() == 0);

  /* looking up old pid after exit + reinit should fail */
  REQUIRE(kfd_process_init() == 0);
  struct kfd_process *found = NULL;
  REQUIRE(kfd_process_find_by_pid(6000, &found) == -2); /* -ENOENT */
  kfd_process_exit();
}

TEST_CASE("kfd_process mm_shim wire-up via create/destroy (Phase C.2.1)",
          "[kfd][process][mm_shim][phase_c][wire_up]") {
  /* SPEC §5: "kfd_process wire-up"
   * "kfd_process_create(pid=0x1001) → process->mm_shim 可查"
   * Invariant: kfd_process_create() 后 kfd_process->mm_shim != NULL
   * Invariant: kfd_process_destroy() 后 mm_shim 被释放（destroy 返回 0） */

  REQUIRE(kfd_process_init() == 0);

  struct kfd_process *p = nullptr;
  REQUIRE(kfd_process_create(&p, 0x1001) == 0);
  REQUIRE(p != nullptr);

  /* mm_shim is opaque void* in kfd_priv.h — verify it was allocated and initialized.
   * Cast to us_mm_shim* (definition from <kernel/uvm/mm_shim.h>) and exercise
   * the public mm_shim API to prove it's a real, initialized us_mm_shim. */
  struct us_mm_shim *shim = static_cast<struct us_mm_shim *>(p->mm_shim);
  CHECK(shim != nullptr);

  /* The mm_shim's PID should match what we passed to create() */
  CHECK(shim->pid == 0x1001UL);

  /* Register a VMA via the shim — verifies it's a valid us_mm_shim */
  int rc = us_mm_shim_register_vma(shim, 0x10000UL, 0x11000UL, 0);
  CHECK(rc == 0);

  /* The registered range should be findable */
  unsigned long out_start = 0, out_end = 0;
  int find_rc = us_mm_shim_find_vma(shim, 0x10500UL, &out_start, &out_end);
  CHECK(find_rc == 0);
  CHECK(out_start == 0x10000UL);
  CHECK(out_end == 0x11000UL);

  /* Destroying must free mm_shim without crashing.
   * After destroy, p (and its mm_shim) is freed; do not dereference. */
  CHECK(kfd_process_destroy(p) == 0);

  kfd_process_exit();
}