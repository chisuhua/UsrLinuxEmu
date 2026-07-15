/*
 * test_kfd_dispatch_standalone.cpp — C-12 B.2.1 dispatch table tests
 *
 * Verifies the KFD ioctl dispatch scaffolding:
 *   - init/exit lifecycle
 *   - dispatch without init returns -EIO
 *   - NULL handlers rejected
 *   - out-of-range cmd returns -ENOSYS
 *   - correct handler routing
 *   - handler return value pass-through
 *   - call count tracking
 *   - concurrent thread-safety
 *   - exit prevents further dispatch
 *
 * Build: links kfd_dispatch.c directly (standalone target).
 */

#include "catch_amalgamated.hpp"

extern "C" {
#include "kfd_dispatch.h"
}

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

/* ── mock handler infrastructure ──────────────────────────────────────────── */

/* Record of which cmd was dispatched to which handler index. */
struct dispatch_record {
    u32 cmd;
    int handler_idx;
    void *arg;
};

/* Shared state for mock handlers (test fixture resets before each test) */
static dispatch_record g_records[KFD_IOC_COUNT * 10];
static std::atomic_int g_record_count{0};
static int g_mock_returns[KFD_IOC_COUNT];  /* per-handler return value */

static void reset_mock_state() {
    g_record_count.store(0);
    std::memset(g_records, 0, sizeof(g_records));
    for (int i = 0; i < KFD_IOC_COUNT; i++)
        g_mock_returns[i] = 0;
}

/* Mock handler: records the call and returns a configurable value. */
static int mock_handler_0(u32 cmd, void *arg) {
    int idx = g_record_count.fetch_add(1);
    if (idx < (int)(sizeof(g_records) / sizeof(g_records[0]))) {
        g_records[idx].cmd = cmd;
        g_records[idx].handler_idx = 0;
        g_records[idx].arg = arg;
    }
    return g_mock_returns[0];
}

static int mock_handler_1(u32 cmd, void *arg) {
    int idx = g_record_count.fetch_add(1);
    if (idx < (int)(sizeof(g_records) / sizeof(g_records[0]))) {
        g_records[idx].cmd = cmd;
        g_records[idx].handler_idx = 1;
        g_records[idx].arg = arg;
    }
    return g_mock_returns[1];
}

static int mock_handler_2(u32 cmd, void *arg) {
    int idx = g_record_count.fetch_add(1);
    if (idx < (int)(sizeof(g_records) / sizeof(g_records[0]))) {
        g_records[idx].cmd = cmd;
        g_records[idx].handler_idx = 2;
        g_records[idx].arg = arg;
    }
    return g_mock_returns[2];
}

static int mock_handler_3(u32 cmd, void *arg) {
    int idx = g_record_count.fetch_add(1);
    if (idx < (int)(sizeof(g_records) / sizeof(g_records[0]))) {
        g_records[idx].cmd = cmd;
        g_records[idx].handler_idx = 3;
        g_records[idx].arg = arg;
    }
    return g_mock_returns[3];
}

static int mock_handler_4(u32 cmd, void *arg) {
    int idx = g_record_count.fetch_add(1);
    if (idx < (int)(sizeof(g_records) / sizeof(g_records[0]))) {
        g_records[idx].cmd = cmd;
        g_records[idx].handler_idx = 4;
        g_records[idx].arg = arg;
    }
    return g_mock_returns[4];
}

static const kfd_ioctl_handler_t *make_mock_handlers() {
    static const kfd_ioctl_handler_t handlers[KFD_IOC_COUNT] = {
        mock_handler_0,
        mock_handler_1,
        mock_handler_2,
        mock_handler_3,
        mock_handler_4,
    };
    return handlers;
}

/* ── per-test fixture ─────────────────────────────────────────────────────── */

struct DispatchFixture {
    DispatchFixture() {
        reset_mock_state();
        /* Ensure clean state: exit any prior test's registration */
        kfd_dispatch_exit();
    }
    ~DispatchFixture() {
        kfd_dispatch_exit();
    }
};

/* ── test cases ──────────────────────────────────────────────────────────── */

TEST_CASE_METHOD(DispatchFixture, "kfd_dispatch init/exit lifecycle",
                 "[kfd][dispatch]") {
    const kfd_ioctl_handler_t *handlers = make_mock_handlers();

    /* Init succeeds with valid handlers */
    int ret = kfd_dispatch_init(handlers);
    REQUIRE(ret == 0);

    /* Dispatch works after init */
    ret = kfd_dispatch(KFD_IOC_CREATE_QUEUE, (void *)0x1000);
    REQUIRE(ret == 0);
    REQUIRE(g_record_count.load() == 1);
    REQUIRE(g_records[0].cmd == KFD_IOC_CREATE_QUEUE);
    REQUIRE(g_records[0].handler_idx == 0);

    /* Exit shuts down dispatch */
    kfd_dispatch_exit();
    ret = kfd_dispatch(KFD_IOC_CREATE_QUEUE, nullptr);
    REQUIRE(ret == -5);  /* -EIO on POSIX */
}

TEST_CASE_METHOD(DispatchFixture, "kfd_dispatch without init returns -EIO",
                 "[kfd][dispatch]") {
    /* No init called — dispatch must return -EIO */
    int ret = kfd_dispatch(KFD_IOC_CREATE_QUEUE, nullptr);
    REQUIRE(ret == -5);  /* -EIO */
}

TEST_CASE_METHOD(DispatchFixture, "kfd_dispatch NULL handlers returns -EINVAL",
                 "[kfd][dispatch]") {
    int ret = kfd_dispatch_init(NULL);
    REQUIRE(ret == -22);  /* -EINVAL on POSIX */

    /* State must remain uninitialized */
    ret = kfd_dispatch(KFD_IOC_CREATE_QUEUE, nullptr);
    REQUIRE(ret == -5);  /* still -EIO */
}

TEST_CASE_METHOD(DispatchFixture, "kfd_dispatch out-of-range cmd returns -ENOSYS",
                 "[kfd][dispatch]") {
    const kfd_ioctl_handler_t *handlers = make_mock_handlers();
    REQUIRE(kfd_dispatch_init(handlers) == 0);

    /* cmd below valid range (0x40 = KFD_IOC_CREATE_QUEUE) */
    int ret = kfd_dispatch(0x3F, nullptr);
    REQUIRE(ret == -38);  /* -ENOSYS */

    /* cmd at upper edge of valid range (0x44 is max with KFD_IOC_COUNT=5) */
    ret = kfd_dispatch(KFD_IOC_UPDATE_QUEUE, nullptr);  /* 0x44, idx=4 */
    REQUIRE(ret == 0);  /* valid */

    /* cmd just past valid range */
    ret = kfd_dispatch(0x45, nullptr);  /* 0x45 = idx 5, out of range */
    REQUIRE(ret == -38);  /* -ENOSYS */

    /* cmd at 0x47 (way past range) */
    ret = kfd_dispatch(KFD_IOC_SET_TRAP_HANDLER, nullptr);  /* 0x47, idx=7 */
    REQUIRE(ret == -38);  /* -ENOSYS */

    /* Verify zero dispatches recorded (out-of-range doesn't invoke handlers) */
    REQUIRE(kfd_dispatch_call_count() == 1);  /* only UPDATE_QUEUE counted */
}

TEST_CASE_METHOD(DispatchFixture, "kfd_dispatch routes to correct handler",
                 "[kfd][dispatch]") {
    const kfd_ioctl_handler_t *handlers = make_mock_handlers();
    REQUIRE(kfd_dispatch_init(handlers) == 0);

    /* Each cmd should route to the handler at (cmd - 0x40) */
    int ret;

    ret = kfd_dispatch(KFD_IOC_CREATE_QUEUE, (void *)0xAAA);
    REQUIRE(ret == 0);
    REQUIRE(g_records[0].handler_idx == 0);
    REQUIRE(g_records[0].cmd == KFD_IOC_CREATE_QUEUE);
    REQUIRE(g_records[0].arg == (void *)0xAAA);

    ret = kfd_dispatch(KFD_IOC_DESTROY_QUEUE, (void *)0xBBB);
    REQUIRE(ret == 0);
    REQUIRE(g_records[1].handler_idx == 1);
    REQUIRE(g_records[1].cmd == KFD_IOC_DESTROY_QUEUE);

    ret = kfd_dispatch(KFD_IOC_SET_MEMORY_POLICY, (void *)0xCCC);
    REQUIRE(ret == 0);
    REQUIRE(g_records[2].handler_idx == 2);
    REQUIRE(g_records[2].cmd == KFD_IOC_SET_MEMORY_POLICY);

    ret = kfd_dispatch(KFD_IOC_GET_PROCESS_APERTURE, (void *)0xDDD);
    REQUIRE(ret == 0);
    REQUIRE(g_records[3].handler_idx == 3);
    REQUIRE(g_records[3].cmd == KFD_IOC_GET_PROCESS_APERTURE);

    ret = kfd_dispatch(KFD_IOC_UPDATE_QUEUE, (void *)0xEEE);
    REQUIRE(ret == 0);
    REQUIRE(g_records[4].handler_idx == 4);
    REQUIRE(g_records[4].cmd == KFD_IOC_UPDATE_QUEUE);

    REQUIRE(kfd_dispatch_call_count() == 5);
}

TEST_CASE_METHOD(DispatchFixture, "kfd_dispatch handler return value passes through",
                 "[kfd][dispatch]") {
    const kfd_ioctl_handler_t *handlers = make_mock_handlers();
    REQUIRE(kfd_dispatch_init(handlers) == 0);

    /* Configure mock to return specific errno values */
    g_mock_returns[0] = -12;  /* -ENOMEM */
    g_mock_returns[1] = -1;   /* -EPERM */
    g_mock_returns[2] = 0;

    int ret = kfd_dispatch(KFD_IOC_CREATE_QUEUE, nullptr);
    REQUIRE(ret == -12);

    ret = kfd_dispatch(KFD_IOC_DESTROY_QUEUE, nullptr);
    REQUIRE(ret == -1);

    ret = kfd_dispatch(KFD_IOC_SET_MEMORY_POLICY, nullptr);
    REQUIRE(ret == 0);

    /* Call count is incremented even on negative returns */
    REQUIRE(kfd_dispatch_call_count() == 3);
}

TEST_CASE_METHOD(DispatchFixture, "kfd_dispatch call count tracks dispatches",
                 "[kfd][dispatch]") {
    const kfd_ioctl_handler_t *handlers = make_mock_handlers();
    REQUIRE(kfd_dispatch_init(handlers) == 0);

    REQUIRE(kfd_dispatch_call_count() == 0);

    kfd_dispatch(KFD_IOC_CREATE_QUEUE, nullptr);
    REQUIRE(kfd_dispatch_call_count() == 1);

    kfd_dispatch(KFD_IOC_DESTROY_QUEUE, nullptr);
    kfd_dispatch(KFD_IOC_SET_MEMORY_POLICY, nullptr);
    REQUIRE(kfd_dispatch_call_count() == 3);

    /* Re-init resets counter */
    kfd_dispatch_exit();
    REQUIRE(kfd_dispatch_init(handlers) == 0);
    REQUIRE(kfd_dispatch_call_count() == 0);
}

TEST_CASE_METHOD(DispatchFixture, "kfd_dispatch concurrent calls are thread-safe",
                 "[kfd][dispatch]") {
    const kfd_ioctl_handler_t *handlers = make_mock_handlers();
    REQUIRE(kfd_dispatch_init(handlers) == 0);

    g_mock_returns[0] = 0;
    g_mock_returns[1] = 0;
    g_mock_returns[2] = 0;
    g_mock_returns[3] = 0;
    g_mock_returns[4] = 0;

    constexpr int kThreads = 4;
    constexpr int kCallsPerThread = 10;

    /* Per-thread error accumulator — avoids REQUIRE inside threads
     * which races on Catch2's RunContext (TSan false positive). */
    std::atomic_int errors_per_thread[kThreads] = {};
    std::atomic_int last_cmd[kThreads] = {};

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; t++) {
        threads.emplace_back([t, &errors_per_thread, &last_cmd]() {
            for (int i = 0; i < kCallsPerThread; i++) {
                u32 cmd = KFD_IOC_CREATE_QUEUE + (u32)((t + i) % KFD_IOC_COUNT);
                last_cmd[t].store((int)cmd, std::memory_order_relaxed);
                int ret = kfd_dispatch(cmd, nullptr);
                if (ret != 0)
                    errors_per_thread[t].fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto &th : threads)
        th.join();

    /* Verify zero dispatch errors (all REQUIRE calls done AFTER join) */
    for (int t = 0; t < kThreads; t++) {
        REQUIRE(errors_per_thread[t].load() == 0);
    }

    /* Total dispatches = kThreads * kCallsPerThread */
    REQUIRE(kfd_dispatch_call_count() == kThreads * kCallsPerThread);

    /* Records should have the correct total (handler_idx entries match) */
    int total = g_record_count.load();
    REQUIRE(total == kThreads * kCallsPerThread);

    /* Verify that record ordering is consistent (no corruption).
     * Each record's handler_idx should be between 0 and 4. */
    int handler_counts[5] = {0, 0, 0, 0, 0};
    for (int i = 0; i < total; i++) {
        REQUIRE(g_records[i].handler_idx >= 0);
        REQUIRE(g_records[i].handler_idx < KFD_IOC_COUNT);
        handler_counts[g_records[i].handler_idx]++;
    }
    /* Each handler should have been called at least once */
    for (int h = 0; h < KFD_IOC_COUNT; h++) {
        REQUIRE(handler_counts[h] > 0);
    }
}

TEST_CASE_METHOD(DispatchFixture, "kfd_dispatch_exit prevents further dispatch",
                 "[kfd][dispatch]") {
    const kfd_ioctl_handler_t *handlers = make_mock_handlers();
    REQUIRE(kfd_dispatch_init(handlers) == 0);

    /* Dispatch works before exit */
    int ret = kfd_dispatch(KFD_IOC_CREATE_QUEUE, nullptr);
    REQUIRE(ret == 0);

    kfd_dispatch_exit();

    /* All commands blocked after exit */
    ret = kfd_dispatch(KFD_IOC_CREATE_QUEUE, nullptr);
    REQUIRE(ret == -5);

    ret = kfd_dispatch(KFD_IOC_DESTROY_QUEUE, nullptr);
    REQUIRE(ret == -5);

    ret = kfd_dispatch(KFD_IOC_UPDATE_QUEUE, nullptr);
    REQUIRE(ret == -5);

    /* Call count not incremented after exit */
    REQUIRE(kfd_dispatch_call_count() == 1);
}

TEST_CASE_METHOD(DispatchFixture, "kfd_dispatch_exit is idempotent",
                 "[kfd][dispatch]") {
    const kfd_ioctl_handler_t *handlers = make_mock_handlers();
    REQUIRE(kfd_dispatch_init(handlers) == 0);

    kfd_dispatch_exit();
    kfd_dispatch_exit();  /* second call must not crash */
    kfd_dispatch_exit();  /* third call must not crash */

    int ret = kfd_dispatch(KFD_IOC_CREATE_QUEUE, nullptr);
    REQUIRE(ret == -5);
}