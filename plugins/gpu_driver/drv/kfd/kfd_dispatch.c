/*
 * kfd_dispatch.c — KFD ioctl dispatch table implementation (C-12 B.2.1)
 *
 * Pure scaffolding: provides a register → lookup → invoke pipeline for
 * KFD ioctl handlers without modifying the existing drm_ioctl_desc[]
 * table (B.2.3 constraint).
 *
 * Lifecycle:
 *   kfd_dispatch_init(handlers)  → register handler array
 *   kfd_dispatch(cmd, arg)       → route to handler (many calls)
 *   kfd_dispatch_exit()          → unregister (idempotent)
 *
 * Thread-safety model (per ADR-060 §2.3):
 *   - handlers_ is write-once at init, read-only afterward
 *   - call_count_ uses C11 atomic fetch_add / load
 *   - kfd_dispatch_exit() sets handlers_ = NULL (lock-free, visible
 *     to concurrent kfd_dispatch callers via __ATOMIC_SEQ_CST)
 */

#include "kfd_dispatch.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stddef.h>   /* NULL */

/* ── static_assert: guard KFD_IOC_COUNT against overflow ─────────────────── */
static_assert(KFD_IOC_COUNT <= 8,
              "KFD_IOC_COUNT must not exceed 8 (cmd space 0x40-0x47)");

/* ── dispatch state ─────────────────────────────────────────────────────── */

/* Pointer to the handler array registered via kfd_dispatch_init().
 * NULL → not initialized or already shut down.
 * After init, this pointer is read-only (no writes after init). */
static const kfd_ioctl_handler_t *handlers_ = NULL;

/* Atomic counter incremented on each successful dispatch call.
 * Exposed via kfd_dispatch_call_count() for test introspection. */
static atomic_int call_count_ = 0;

/* ── public API ─────────────────────────────────────────────────────────── */

int kfd_dispatch_init(const kfd_ioctl_handler_t *handlers) {
    if (!handlers)
        return -EINVAL;

    /* Store the handler array.  We do NOT take ownership — the caller
     * must keep the array alive until kfd_dispatch_exit(). */
    __atomic_store_n(&handlers_, handlers, __ATOMIC_SEQ_CST);

    /* Reset call counter on (re-)init. */
    atomic_store(&call_count_, 0);

    return 0;
}

int kfd_dispatch(u32 cmd, void *arg) {
    /* Read handlers_ with acquire semantics.  If NULL, dispatch has
     * not been initialized or has been shut down. */
    const kfd_ioctl_handler_t *h =
        __atomic_load_n(&handlers_, __ATOMIC_ACQUIRE);

    if (!h)
        return -EIO;

    /* Map cmd to array index: KFD_IOC_CREATE_QUEUE (0x40) → index 0 */
    int idx = (int)(cmd - KFD_IOC_CREATE_QUEUE);

    if (idx < 0 || idx >= KFD_IOC_COUNT)
        return -ENOSYS;

    int ret = h[idx](cmd, arg);

    /* Increment call counter ONLY after successful routing.
     * The handler may return a negative errno; we still count the
     * dispatch as "successful" (the handler was invoked). */
    atomic_fetch_add(&call_count_, 1);

    return ret;
}

void kfd_dispatch_exit(void) {
    __atomic_store_n(&handlers_, NULL, __ATOMIC_SEQ_CST);
}

int kfd_dispatch_call_count(void) {
    return atomic_load(&call_count_);
}