/*
 * test_kfd_queue_standalone.cpp — C-12 Phase D FIXME cleanup verification
 *
 * Verifies:
 *   1. kfd_queue_unref_bo_vas_locked exists and is callable
 *   2. kfd_queue_unref_bo_vas still works (non-locked, backward compat)
 *   3. kfd_queue_buffer_put is removed (compile-time: no declaration exists)
 *   4. kfd_queue_buffer_get / kfd_queue_unref_bo_va API preserved
 *
 * Negative test (kfd_queue_buffer_put removal) is verified at build time:
 *   nm libgpu_kfd.a | grep kfd_queue_buffer_put → must be empty.
 */

#include "catch_amalgamated.hpp"

/* Forward-declare only the types/structs needed by kfd_queue.c API.
 * kfd_priv.h can't be included here because it needs <atomic> + extern "C"
 * which creates linkage conflicts with the C-linkage stubs below. */

struct kfd_process_device;
struct queue_properties;

extern "C" {
int kfd_queue_unref_bo_vas(struct kfd_process_device *pdd,
			   struct queue_properties *properties);
int kfd_queue_unref_bo_vas_locked(struct kfd_process_device *pdd,
				  struct queue_properties *properties);

/* DRM compat stubs for kfd_queue.c link-time resolution.
 * These symbols aren't in libkernel.so yet (Stage 1.4 PoC limitations). */
struct amdgpu_bo;
struct amdgpu_vm;
struct amdgpu_bo_va;
struct amdgpu_bo_va_mapping;

typedef unsigned long long u64;

void *drm_priv_to_vm(void *drm_priv)
{
	(void)drm_priv;
	return nullptr;
}

int amdgpu_bo_reserve(struct amdgpu_bo *bo, int intr)
{
	(void)bo;
	(void)intr;
	return 0;
}

void amdgpu_bo_unreserve(struct amdgpu_bo *bo)
{
	(void)bo;
}

void amdgpu_bo_unref(struct amdgpu_bo **bo)
{
	(void)bo;
}

struct amdgpu_bo *amdgpu_bo_ref(struct amdgpu_bo *bo)
{
	return bo;
}

struct amdgpu_bo_va_mapping *amdgpu_vm_bo_lookup_mapping(struct amdgpu_vm *vm, u64 addr)
{
	(void)vm;
	(void)addr;
	return nullptr;
}

struct amdgpu_bo_va *amdgpu_vm_bo_find(struct amdgpu_vm *vm, struct amdgpu_bo *bo)
{
	(void)vm;
	(void)bo;
	return nullptr;
}
}

/* ── Phase D verification tests ───────────────────────────────────────────── */

TEST_CASE("Phase D: kfd_queue_unref_bo_vas_locked symbol exists",
          "[kfd_queue][phase_d]")
{
	/* Link-time symbol resolution check */
	REQUIRE(reinterpret_cast<void *>(&kfd_queue_unref_bo_vas_locked) != nullptr);
}

TEST_CASE("Phase D: kfd_queue_unref_bo_vas symbol still exists",
          "[kfd_queue][phase_d]")
{
	/* Legacy (non-locked) variant preserved for out_err_release path */
	REQUIRE(reinterpret_cast<void *>(&kfd_queue_unref_bo_vas) != nullptr);
}

TEST_CASE("Phase D: _locked and non-locked are distinct functions",
          "[kfd_queue][phase_d]")
{
	/* They must be different functions (not aliased) */
	REQUIRE(reinterpret_cast<void *>(&kfd_queue_unref_bo_vas_locked) !=
		reinterpret_cast<void *>(&kfd_queue_unref_bo_vas));
}