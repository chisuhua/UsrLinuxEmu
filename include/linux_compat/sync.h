#pragma once

#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include "types.h"

// Linux同步机制兼容层（基于POSIX线程实现）

// ============================================================
// 自旋锁（用互斥锁模拟）
// ============================================================
typedef pthread_mutex_t spinlock_t;

#define SPIN_LOCK_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define DEFINE_SPINLOCK(x) spinlock_t x = SPIN_LOCK_UNLOCKED

static inline void spin_lock_init(spinlock_t *lock) {
    pthread_mutex_init(lock, nullptr);
}

static inline void spin_lock(spinlock_t *lock) {
    pthread_mutex_lock(lock);
}

static inline void spin_unlock(spinlock_t *lock) {
    pthread_mutex_unlock(lock);
}

static inline int spin_trylock(spinlock_t *lock) {
    return pthread_mutex_trylock(lock) == 0;
}

static inline void spin_lock_destroy(spinlock_t *lock) {
    pthread_mutex_destroy(lock);
}

// IRQ保存版本（用户态中irq标志无意义，忽略flags参数）
#define spin_lock_irqsave(lock, flags)      \
    do { (void)(flags); spin_lock(lock); } while (0)

#define spin_unlock_irqrestore(lock, flags) \
    do { (void)(flags); spin_unlock(lock); } while (0)

#define spin_lock_irq(lock)     spin_lock(lock)
#define spin_unlock_irq(lock)   spin_unlock(lock)

// ============================================================
// 互斥锁
// ============================================================
struct mutex {
    pthread_mutex_t lock;
};

#define DEFINE_MUTEX(x) struct mutex x = { .lock = PTHREAD_MUTEX_INITIALIZER }

static inline void mutex_init(struct mutex *m) {
    pthread_mutex_init(&m->lock, nullptr);
}

static inline void mutex_destroy(struct mutex *m) {
    pthread_mutex_destroy(&m->lock);
}

static inline void mutex_lock(struct mutex *m) {
    pthread_mutex_lock(&m->lock);
}

static inline void mutex_unlock(struct mutex *m) {
    pthread_mutex_unlock(&m->lock);
}

static inline int mutex_trylock(struct mutex *m) {
    return pthread_mutex_trylock(&m->lock) == 0;
}

static inline int mutex_is_locked(struct mutex *m) {
    int ret = pthread_mutex_trylock(&m->lock);
    if (ret == 0) {
        pthread_mutex_unlock(&m->lock);
        return 0;
    }
    return 1;
}

// ============================================================
// 读写锁
// ============================================================
typedef pthread_rwlock_t rwlock_t;

#define DEFINE_RWLOCK(x) rwlock_t x = PTHREAD_RWLOCK_INITIALIZER

static inline void rwlock_init(rwlock_t *lock) {
    pthread_rwlock_init(lock, nullptr);
}

static inline void read_lock(rwlock_t *lock) {
    pthread_rwlock_rdlock(lock);
}

static inline void read_unlock(rwlock_t *lock) {
    pthread_rwlock_unlock(lock);
}

static inline void write_lock(rwlock_t *lock) {
    pthread_rwlock_wrlock(lock);
}

static inline void write_unlock(rwlock_t *lock) {
    pthread_rwlock_unlock(lock);
}

// ============================================================
// 信号量
// ============================================================
struct semaphore {
    sem_t sem;
};

static inline void sema_init(struct semaphore *sem, int val) {
    sem_init(&sem->sem, 0, (unsigned int)val);
}

static inline void down(struct semaphore *sem) {
    sem_wait(&sem->sem);
}

static inline int down_interruptible(struct semaphore *sem) {
    return sem_wait(&sem->sem) == 0 ? 0 : -EINTR;
}

static inline int down_trylock(struct semaphore *sem) {
    return sem_trywait(&sem->sem) == 0 ? 0 : 1;
}

static inline void up(struct semaphore *sem) {
    sem_post(&sem->sem);
}

// ============================================================
// 完成量（completion）
// ============================================================
struct completion {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             done;
};

static inline void init_completion(struct completion *c) {
    pthread_mutex_init(&c->mutex, nullptr);
    pthread_cond_init(&c->cond, nullptr);
    c->done = 0;
}

static inline void complete(struct completion *c) {
    pthread_mutex_lock(&c->mutex);
    c->done++;
    pthread_cond_signal(&c->cond);
    pthread_mutex_unlock(&c->mutex);
}

static inline void complete_all(struct completion *c) {
    pthread_mutex_lock(&c->mutex);
    c->done = 1;
    pthread_cond_broadcast(&c->cond);
    pthread_mutex_unlock(&c->mutex);
}

static inline void wait_for_completion(struct completion *c) {
    pthread_mutex_lock(&c->mutex);
    while (!c->done) {
        pthread_cond_wait(&c->cond, &c->mutex);
    }
    c->done--;
    pthread_mutex_unlock(&c->mutex);
}

static inline int wait_for_completion_interruptible(struct completion *c) {
    wait_for_completion(c);
    return 0;
}

static inline void reinit_completion(struct completion *c) {
    pthread_mutex_lock(&c->mutex);
    c->done = 0;
    pthread_mutex_unlock(&c->mutex);
}
