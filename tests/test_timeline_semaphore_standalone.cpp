// test_timeline_semaphore_standalone.cpp
// Timeline semaphore tests for Stage 4.5 (ADR-049)

#include "catch_amalgamated.hpp"
#include "sim/semaphore_manager.h"

TEST_CASE("sem_create allocates and returns valid handle", "[sem]") {
    SemaphoreManager mgr;
    uint64_t h1 = mgr.create(0);
    REQUIRE(h1 != 0);
    REQUIRE(mgr.query(h1) == 0);

    uint64_t h2 = mgr.create(5);
    REQUIRE(h2 != 0);
    REQUIRE(h2 != h1);
    REQUIRE(mgr.query(h2) == 5);
}

TEST_CASE("sem_signal monotonic enforcement", "[sem]") {
    SemaphoreManager mgr;
    uint64_t h = mgr.create(0);
    REQUIRE(mgr.signal(h, 1) == 0);
    REQUIRE(mgr.query(h) == 1);
    REQUIRE(mgr.signal(h, 1) == -EINVAL);  // equal -> reject
    REQUIRE(mgr.query(h) == 1);             // unchanged
    REQUIRE(mgr.signal(h, 0) == -EINVAL);  // lower -> reject
    REQUIRE(mgr.query(h) == 1);             // unchanged
}

TEST_CASE("sem_wait FIFO ordering", "[sem]") {
    SemaphoreManager mgr;
    uint64_t h = mgr.create(0);
    std::vector<int> order;
    mgr.wait(h, 2, [&order](uint64_t) { order.push_back(1); }, 0);
    mgr.wait(h, 2, [&order](uint64_t) { order.push_back(2); }, 0);
    mgr.signal(h, 2);
    REQUIRE(order.size() == 2);
    REQUIRE(order[0] == 1);
    REQUIRE(order[1] == 2);
}

TEST_CASE("sem_wait fires immediately when condition already met", "[sem]") {
    SemaphoreManager mgr;
    uint64_t h = mgr.create(5);
    bool fired = false;
    mgr.wait(h, 3, [&fired](uint64_t) { fired = true; }, 0);
    REQUIRE(fired);
}

TEST_CASE("sem_destroy invalid handle", "[sem]") {
    SemaphoreManager mgr;
    REQUIRE(mgr.destroy(999) == -EINVAL);
    REQUIRE(mgr.query(999) == UINT64_MAX);
    REQUIRE(mgr.signal(999, 1) == -EINVAL);
}

TEST_CASE("sem_destroy wakes waiters", "[sem]") {
    SemaphoreManager mgr;
    uint64_t h = mgr.create(0);
    bool woken = false;
    mgr.wait(h, 1, [&woken](uint64_t) { woken = true; }, 0);
    mgr.destroy(h);
    REQUIRE(woken);
    // Double destroy returns error
    REQUIRE(mgr.destroy(h) == -EINVAL);
}

TEST_CASE("sem_query on invalid handle", "[sem]") {
    SemaphoreManager mgr;
    REQUIRE(mgr.query(0) == UINT64_MAX);
    REQUIRE(mgr.query(99999) == UINT64_MAX);
}

TEST_CASE("multiple waiters at different thresholds", "[sem]") {
    SemaphoreManager mgr;
    uint64_t h = mgr.create(0);
    int count1 = 0, count2 = 0;
    mgr.wait(h, 1, [&count1](uint64_t) { count1++; }, 0);
    mgr.wait(h, 2, [&count2](uint64_t) { count2++; }, 0);
    mgr.signal(h, 1);
    REQUIRE(count1 == 1);
    REQUIRE(count2 == 0);
    mgr.signal(h, 2);
    REQUIRE(count1 == 1);
    REQUIRE(count2 == 1);
}
