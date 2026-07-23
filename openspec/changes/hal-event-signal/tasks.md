# hal-event-signal: 实施任务

## 1. Sim Event Ops (hal_mock.cpp)
- [ ] 1.1 实现 `event_signal` — 通知等待者
- [ ] 1.2 实现 `event_wait` — 阻塞等待信号
- [ ] 1.3 实现 `event_notify` — 广播通知

## 2. Hw Event Ops (hal_user.cpp)
- [ ] 2.1 实现 `event_signal` 真机版本
- [ ] 2.2 实现 `event_wait` 真机版本
- [ ] 2.3 实现 `event_notify` 真机版本

## 3. 测试
- [ ] 3.1 编写 `test_hal_event_signal_wait` — 单线程 signal/wait
- [ ] 3.2 编写 `test_hal_event_concurrent` — 多线程并发