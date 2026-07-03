# DRM 兼容矩阵：Linux 6.6 LTS ↔ 6.12 LTS

> **目的**：记录 DRM / GEM 子集在 Linux 6.6 与 6.12 LTS 之间的 API 差异，为 Stage 1.2 锁定 LTS 提供依据。
> **创建日期**：2026-07-02（盲点 5 决策）
> **目标 LTS**：**Linux 6.12 LTS**（Lock）
> **源依据**：librarian 验证 + Linux kernel 头文件 `include/linux/dma-buf.h` / `include/drm/drm_*.h`

---

## 1. 总体结论

| 维度 | 6.6 LTS | 6.12 LTS | 差异影响 | UsrLinuxEmu 策略 |
|------|---------|----------|---------|-----------------|
| 整体 ABI 兼容 | — | — | **几乎完全兼容** | 默认按 6.12 实现，6.6 行为差异按需按 ADR-027 增量补 |
| dma-buf API 集 | 成熟 | 微调 | 关键修正：amdgpu 用 `dynamic_attach` | 见 §2 |
| GEM object 生命周期 | 一致 | 一致 | 无变更 | 完整实现 |
| `drm_ioctl_desc[]` | 一致 | 一致 | 无变更 | 沿用 |
| Render node 权限 | `0666` 默认 | `0666` 默认 | 无变更 | 对齐 udev（ADR-037）|

---

## 2. dma-buf API 集差异（**关键修正**）

### 2.1 函数列表

| 函数 | 6.6 存在？ | 6.12 存在？ | KFD 实际调用？ |
|------|----------|-----------|----------------|
| `dma_buf_attach()` | ✅ | ✅ | ❌ amdgpu **不调用** |
| `dma_buf_dynamic_attach()` | ✅ | ✅ | ✅ `amdgpu_dma_buf.c:570` |
| `dma_buf_detach()` | ✅ | ✅ | ✅ |
| `dma_buf_map_attachment()` | ✅ | ✅ | ✅ `amdgpu_ttm.c` |
| `dma_buf_unmap_attachment()` | ✅ | ✅ | ✅ `amdgpu_ttm.c` |
| `dma_buf_pin()` / `dma_buf_unpin()` | ✅ | ✅ | ✅ |
| `dma_buf_export()` | ✅ | ✅ | ✅（强制验证 3 ops） |

### 2.2 结论

**Stage 1.2 `drm_prime.h` 必须实现的 API 子集**：

```c
struct dma_buf *dma_buf_export(...);
struct dma_buf_attachment *dma_buf_dynamic_attach(
    struct dma_buf *dmabuf, struct device *dev,
    const struct dma_buf_attach_ops *importer_ops,
    void *importer_priv);
void dma_buf_detach(struct dma_buf *dmabuf, struct dma_buf_attachment *attach);
struct sg_table *dma_buf_map_attachment(
    struct dma_buf_attachment *attach, enum dma_data_direction dir);
void dma_buf_unmap_attachment(struct dma_buf_attachment *attach,
                              struct sg_table *sg_table);
int dma_buf_pin(struct dma_buf_attachment *attach);
int dma_buf_unpin(struct dma_buf_attachment *attach);
```

**`dma_buf_attach()` 不实现**（amdgpu 不调；保持 ABI 清洁）

### 2.3 `dma_buf_export` 强制验证

`dma_buf_export` 在两版本中均强制验证 3 ops 必须非 NULL：

```c
if (WARN_ON(!exp_info->priv || !exp_info->ops
        || !exp_info->ops->map_dma_buf    /* 必须 */
        || !exp_info->ops->unmap_dma_buf  /* 必须 */
        || !exp_info->ops->release))       /* 必须 */
    return ERR_PTR(-EINVAL);
```

**Stage 1.2 `drm_prime.cpp` `map_dma_buf` 必须返回有效 `sg_table`，不能 `-ENOSYS`**；IOMMU `map_page` 可旁路（恒等映射）。

---

## 3. struct layout 差异

### 3.1 `struct dma_buf` 条件化

| 字段 | 6.6 | 6.12 |
|------|-----|------|
| `list_node` | **始终存在**（无条件） | **条件编译** `#if IS_ENABLED(CONFIG_DEBUG_FS)` |

**影响**：仅当 `CONFIG_DEBUG_FS=n` 时 `struct dma_buf` 大小/字段偏移在 6.6 与 6.12 之间不同。

**UsrLinuxEmu 策略**：
- 默认编译选项开启 `CONFIG_DEBUG_FS=y`，差异不可见
- 在 `drm_prime.h` 中保留 `list_node` 字段（条件化版本，避免 Linux 6.6 内核编译器报错）

### 3.2 其他结构体

| 类型 | 6.6 ↔ 6.12 |
|------|------------|
| `struct dma_buf_ops` | 无字段差异（3 个 mandatory ops 一致） |
| `struct dma_buf_attachment` | 无字段差异 |
| `struct dma_buf_attach_ops` | 无字段差异（`allow_peer2peer` + `move_notify` 一致） |

---

## 4. 函数签名差异

| 函数 | 6.6 vs 6.12 |
|------|-----------|
| 全部 dma-buf API 函数 | **无差异** |
| `DRM_IOCTL_DEF_DRV` 宏 | 无差异 |
| DRM 主线 ioctl 编号 | 无差异 |

---

## 5. 新增必须 ops 差异

**无**。`struct dma_buf_ops` 的 mandatory ops 集 (`map_dma_buf` / `unmap_dma_buf` / `release`) 在两版本中完全一致。

---

## 6. 兼容性总结表

| 类别 | 6.6 → 6.12 兼容性 | Stage 1.2 处理 |
|------|----------------|---------------|
| dma_buf_attach 签名 | ✅ 无变化 | 不实现（KFD 不用）|
| dma_buf_dynamic_attach | ✅ 无变化 | **实现** |
| `struct dma_buf.list_node` | ⚠️ 条件化 | 头文件中按条件化保留 |
| `struct dma_buf_ops` mandatory 集 | ✅ 无变化 | 全部实现 |
| `struct dma_buf_attach_ops` | ✅ 无变化 | 完整实现 |
| DRM ioctl 派发机制 | ✅ 无变化 | 沿用 |
| KFD 5 个 ioctl 集 | ✅ 无变化 | 已在 System C 预留 |

---

## 7. 引用

- Linux 6.12 LTS 头文件：`include/linux/dma-buf.h`、`include/drm/drm_*.h`
- AMD KFD 引用：`drivers/gpu/drm/amd/amdkfd/kfd_priv.h`、`amdgpu_dma_buf.c:570`
- ADR-027 spec-driven 增量原则
- Oracle 评估（2026-07-02）盲点 5 决策

---

**维护者**：UsrLinuxEmu Architecture Team
**最后更新**：2026-07-02
**对应 SSOT**：`docs/02_architecture/post-refactor-architecture.md §1.10`
