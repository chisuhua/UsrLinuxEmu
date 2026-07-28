# Semaphore & Barrier

## Requirements

### R1: Semaphore WAIT
- Puller FSM FETCH 阶段：遇到 `SEM_WAIT` 类型 entry，阻塞直到 `mem_read(semaphore_va) >= semaphore_value`
- 实现方式：将 entry 临时移出活跃队列放入 pending 队列，Puller 在每轮 FETCH 循环中检查 pending 队列条件
- WAIT 模式下不阻塞 Puller 处理其他 entry（多 channel 场景）；单 channel 场景通道内 WAIT 之前的 entry 未完成时后续不 dispatch

### R2: Semaphore RELEASE
- Puller FSM COMPLETE 阶段：batch 完成后执行 `mem_write(semaphore_va, semaphore_value)`
- RELEASE 不阻塞——写完成后立即继续
- 如果 batch 中同时包含 RELEASE 和其他 entry，RELEASE 在 batch 的最后一个 entry 完成后执行

### R3: Barrier AND
- `BARRIER_AND` 类型 entry：等待所有指定 stream 到达 barrier 点后才放行
- Barrier 计数器：N 个 stream 各 signal 一次后，counter 归零，所有等待 entry 放行

### R4: Barrier OR
- `BARRIER_OR` 类型 entry：任一指定 stream 到达 barrier 点即放行
- 第一个 stream signal 后立即放行，后续 entry 的 signal 被忽略

### R5: Verifiability
- `test_semaphore_barrier_standalone`：WAIT/RELEASE 序列化 + Barrier AND + Barrier OR
- WAIT 超时场景：semaphore 从未 signal 时 entry 保持 pending 状态（不崩溃）
