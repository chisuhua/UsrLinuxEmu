# Priority Scheduling

## Requirements

### R1: Channel Priority Field
- `ChannelState`（ADR-044）新增 `priority` 字段，类型 `ChannelPriority` 枚举
- 枚举值：`IDLE=0`, `LOW=1`, `NORMAL=2`, `HIGH=3`, `REALTIME=4`
- 默认值：`NORMAL`
- Priority 在 Queue 创建时设置，运行期不可变

### R2: Runlist Reordering
- `GlobalScheduler::dispatch_next()` 出队时按优先级降序选择（高优先级优先）
- 同级优先级的 entry 按 FIFO 顺序
- Starvation 保护：当 `HIGH/REALTIME` entry 持续存在时，`LOW/NORMAL` entry 的连续跳过次数触发回退机制——每 10 次 dispatch 周期，强制至少 dispatch 1 个最低优先级 entry

### R3: Priority Inheritance
- 当 `REALTIME` entry 因 semaphore WAIT 阻塞时，若该 semaphore 由 `LOW` entry signal，`LOW` entry 临时提升到 `HIGH` 优先级（防止优先级反转）

### R4: Verifiability
- `test_priority_sched_standalone`：提交高/中/低 3 个 queue，验证高优先级先完成
- Starvation 保护场景：持续提交 high entry，验证 low entry 不被无限期推迟
