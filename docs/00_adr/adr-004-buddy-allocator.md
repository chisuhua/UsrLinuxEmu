# ADR-004: 使用 Buddy Allocator 管理 GPU 内存

**状态**: ✅ 已接受 (Accepted)

**日期**: 2025-12

## 背景

GPU 需要高效的内存管理机制，既要支持各种大小的内存分配，又要减少内存碎片。

## 决策

使用 Buddy Allocator（伙伴分配器）算法。

## 理由

1. **高效**: O(log n) 的分配和释放时间
2. **碎片管理**: 自动合并相邻空闲块，减少外部碎片
3. **简单**: 算法相对简单，易于实现和调试
4. **经典**: Linux 内核也使用类似算法
5. **可预测**: 性能特性清晰可预测

## 替代方案考虑

- **Slab Allocator**: 适合固定大小对象，不适合变长分配
- **Doug Lea Allocator**: 更复杂，但灵活性更好
- **简单链表**: 太慢，碎片严重

## 与 ADR-015 的关系说明

- ADR-015 确立了 System C (`GPU_IOCTL_*`) 为 canonical 接口，其中 `GPU_IOCTL_ALLOC_BO` 需要支持 memory domain (VRAM/GTT/CPU)
- Buddy Allocator 适用于 **VRAM 内部**的内存管理（作为子分配器），但 domain 选择层在 Buddy Allocator 之上
- 内存分配流程：`ALLOC_BO (domain 选择) → Buddy Allocator (VRAM 内部分配)`

## 后果

- ✅ 高效的内存分配
- ✅ 良好的碎片管理
- ✅ 实现相对简单
- ⚠️ 只能分配 2 的幂次大小（有内部碎片）
- ⚠️ 需要额外的元数据空间

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2025-12