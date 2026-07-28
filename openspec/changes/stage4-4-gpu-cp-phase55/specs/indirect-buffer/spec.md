# Indirect Buffer

## Requirements

### R1: JUMP Instruction
- GPFIFO entry 新增 `type` 枚举值 `IB_JUMP`
- `IB_JUMP` entry 包含 `target_gpu_va`（目标 pushbuffer 地址）和 `continue_flag`（JUMP 完成后是否继续原 batch 的后续 entry）
- 当 `continue_flag=false`：JUMP 完成后终止当前 batch
- 当 `continue_flag=true`：JUMP 完成后返回当前位置继续 FETCH（链式 IB）

### R2: Puller JUMP Behavior
- Puller FETCH 阶段遇到 `IB_JUMP`：保存当前 fetch 状态（`saved_pc`），切换到 `target_gpu_va` 继续 FETCH
- Jump target 必须是合法的 GPU VA（已在 VA Space 中映射）；非法地址返回 `-EFAULT`
- 嵌套深度限制：最多 4 级嵌套 JUMP（`continue_flag=true` 链），超过返回 `-E2BIG`

### R3: IB Reference Management
- `submitBatch` 新增可选 `ib_refs` 字段：指向 `gpu_ib_ref` 结构体数组
- `gpu_ib_ref` 包含 `gpu_va`, `size`, 和 `flags`（read-only flag 等）
- IB reference 生命周期：随 batch 完成自动释放

### R4: Verifiability
- `test_indirect_buffer_standalone`：single JUMP + chained JUMP + 非法 target
- Memory leak 检测：IB reference 在 batch 完成后正确释放
