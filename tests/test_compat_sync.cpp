// 测试同步机制兼容层
#include <iostream>
#include <cassert>
#include <pthread.h>

#include "../include/linux_compat/compat.h"

// 测试自旋锁
void test_spinlock() {
    std::cout << "Testing spinlock...\n";

    spinlock_t lock;
    spin_lock_init(&lock);

    // 基本加锁/解锁
    spin_lock(&lock);
    spin_unlock(&lock);

    // trylock测试
    int ret = spin_trylock(&lock);
    assert(ret == 1); // 应该成功
    spin_unlock(&lock);

    spin_lock_destroy(&lock);
    std::cout << "Spinlock test passed.\n";
}

// 测试互斥锁
void test_mutex() {
    std::cout << "Testing mutex...\n";

    struct mutex m;
    mutex_init(&m);

    // 基本加锁/解锁
    mutex_lock(&m);
    mutex_unlock(&m);

    // trylock测试
    int ret = mutex_trylock(&m);
    assert(ret == 1); // 应该成功
    mutex_unlock(&m);

    mutex_destroy(&m);
    std::cout << "Mutex test passed.\n";
}

// 测试信号量
void test_semaphore() {
    std::cout << "Testing semaphore...\n";

    struct semaphore sem;
    sema_init(&sem, 1);

    // down操作
    down(&sem);

    // trylock（应该失败，因为已经down了）
    int ret = down_trylock(&sem);
    assert(ret != 0); // 应该失败

    // up操作
    up(&sem);

    // 现在trylock应该成功
    ret = down_trylock(&sem);
    assert(ret == 0);
    up(&sem);

    std::cout << "Semaphore test passed.\n";
}

// 测试完成量
void test_completion() {
    std::cout << "Testing completion...\n";

    struct completion c;
    init_completion(&c);

    // 完成信号
    complete(&c);

    // 等待完成（应该立即返回）
    wait_for_completion(&c);

    // 重置完成量
    reinit_completion(&c);

    std::cout << "Completion test passed.\n";
}

// 多线程测试数据
static spinlock_t g_lock;
static int g_counter = 0;
static const int THREAD_COUNT = 4;
static const int ITERATIONS = 1000;

static void *thread_increment(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        spin_lock(&g_lock);
        g_counter++;
        spin_unlock(&g_lock);
    }
    return nullptr;
}

// 测试多线程安全
void test_thread_safety() {
    std::cout << "Testing thread safety with spinlock...\n";

    spin_lock_init(&g_lock);
    g_counter = 0;

    pthread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], nullptr, thread_increment, nullptr);
    }
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], nullptr);
    }

    assert(g_counter == THREAD_COUNT * ITERATIONS);
    spin_lock_destroy(&g_lock);

    std::cout << "Thread safety test passed (counter=" << g_counter << ").\n";
}

int main() {
    std::cout << "Starting sync compatibility tests...\n";

    test_spinlock();
    test_mutex();
    test_semaphore();
    test_completion();
    test_thread_safety();

    std::cout << "All sync compatibility tests passed!\n";
    return 0;
}
