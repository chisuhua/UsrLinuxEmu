# ADR-037: VFS Device Permission Model (Render Node 权限分离)

**状态**: 🔄 提议中 (Proposed)

**日期**: 2026-07-02

**提案人**: Sisyphus（基于 Stage 1.2 DRM 子集启动准备 + Oracle 评估盲点 4）

**评审者**: UsrLinuxEmu Architecture Team

**关联 ADR**: ADR-035 (Governance Policy), ADR-036 (3-Way Separation), ADR-023 (HAL Interface)

**关联 Change**: `openspec/changes/stage-1-2-drm-subset/`

**追踪文档**: `docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md` §Sub-stage 1.2

---

## 背景

UsrLinuxEmu 现有 VFS 实现**完全没有权限基础设施**：

| 当前状态 | 证据 |
|---------|------|
| `Device` 结构体 | 仅 4 字段：`name`、`dev_id`、`plugin_handle`、`fops`（无 `mode_t`/`uid_t`/`gid_t`） |
| `VFS::open()` | 纯字符串查找（剥离 `/dev/` → 精确匹配 `unordered_map`），零权限检查 |
| 多段路径 | **不支持** — `/dev/dri/renderD128` 无法匹配（当前仅剥离单段 `/dev/`） |
| `chmod`/`chown`/`access` | **不存在** — VFS 无任何权限管理接口 |
| 权限常量 | grep `0666`/`0660`/`i_mode`/`chmod` 全仓零命中 |

Stage 1.2（DRM 子集）需要创建 render node `/dev/dri/renderD128` 和 primary node `/dev/dri/card0`，遵循 Linux DRM 权限分离约定。同时 KFD/amdgpu 驱动代码编译时可能调用 `chmod`/`chown`/`fchmod`/`access` 等 POSIX 权限 API。

**核心矛盾**：UsrLinuxEmu 是用户态单用户环境，无真实 root/user 分离，但也需要让 KFD 代码"零修改编译"通过权限 API 调用。

---

## 决策

### 决策 1: 扩展 `Device` 结构体，添加模式字

在 `include/kernel/device/device.h` 的 `Device` 结构体增加三个字段：

```cpp
class Device {
 public:
  Device(const std::string& name, dev_t id,
         std::shared_ptr<FileOperations> ops, void* handle,
         mode_t mode = 0666,      // 新增：文件模式
         uid_t  uid  = 0,         // 新增：所有者 UID
         gid_t  gid  = 0);        // 新增：所有者 GID

  std::string name;
  dev_t dev_id;
  void* plugin_handle = nullptr;
  std::shared_ptr<FileOperations> fops;

  mode_t mode;     // 新增
  uid_t  uid;      // 新增
  gid_t  gid;      // 新增
};
```

**默认值**：`mode = 0666`、`uid = 0`、`gid = 0`（兼容 Linux udev 默认渲染节点权限）。

**向后兼容性**：新增字段有默认值，现有代码无需修改（构造函数调用添加默认参数即可）。

### 决策 2: 多段路径解析（支持 `/dev/dri/renderD128`）

扩展 `VFS::open()` 的路径解析逻辑：

```cpp
std::shared_ptr<Device> VFS::open(const std::string& path, int flags) {
  // 1. 剥离 /dev/ 前缀（保持兼容）
  // 2. 将剩余路径按 '/' 分段
  // 3. 按完整路径（含子目录前缀，如 "dri/renderD128"）查找 device registry
}
```

**registry key 策略**：`devices_` map 的 key 从纯设备名（如 `gpgpu0`）扩展为含子目录前缀的完整路径（如 `dri/renderD128`）。**不引入真实目录 inode 层级**（避免过度工程化）。

**向后兼容性**：现有设备（`/dev/gpgpu0`）注册为 key=`"gpgpu0"`，不受影响。

### 决策 3: 权限检查 hook（no-op，预留扩展点）

在 `VFS::open()` 中 `fops->open()` **之前**插入权限检查调用：

```cpp
// 权限检查 hook（Stage 1.2: 仅编译占位，不强执行）
if (!check_permission(dev, flags)) {
  // 当前始终返回 true（单用户环境）
  // 未来可扩展为：检查 mode bits vs uid/gid
}
```

**为什么是 no-op 而非完全跳过**：确保 KFD 代码中 `open()` + `fchmod()` 等调用路径可编译通过。hook 的存在也标记了"这里有权限检查的意图"，方便未来硬化。

### 决策 4: `chmod`/`chown`/`fchmod`/`access` 接口

在 `VFS` 类中添加以下方法：

| 方法 | 语义 | Stage 1.2 实现 |
|------|------|---------------|
| `VFS::chmod(const std::string& path, mode_t mode)` | 修改设备模式字 | 更新 `Device::mode` 字段 |
| `VFS::chown(const std::string& path, uid_t uid, gid_t gid)` | 修改设备所有者 | 更新 `Device::uid`/`gid` 字段（**不强校验调用者身份**） |
| `VFS::fchmod(int fd, mode_t mode)` | 按 fd 修改模式字 | 通过 `fd_to_device()` 查表后更新 |
| `VFS::access(const std::string& path, int amode)` | 检查访问权限 | 比对 `Device::mode` 位（**不强校验 uid/gid**） |

**不强校验调用者身份**（决策 3 的原则延续）：UsrLinuxEmu 是单用户环境，所有"权限检查"仅做模式位比对（`mode & amode`），不涉及 UID/GID 的真实验证。这保证 KFD 编译通过 + 基础语义正确。

### 决策 5: 默认设备权限（render node vs primary node）

| 设备 | 模式 | 说明 |
|------|------|------|
| `/dev/dri/renderD128` | `0666` | Linux udev 默认：所有用户可读写（无需 DRM_MASTER） |
| `/dev/dri/card0` | `0666` | Linux udev 默认：所有用户可读写（DRM_MASTER 由 ioctl 层控制） |
| `/dev/gpgpu0`（现有） | `0666` | 向后兼容，默认保持不变 |

**不在 Stage 1.2 实现的**：
- DRM_MASTER 认证（`drmSetMaster`/`drmDropMaster`）— 留到 1.4 KFD 集成验证时再评估
- 真实 UID/GID 隔离 — 单用户环境无意义
- `dev_t` 动态分配（主/次设备号）— 使用固定的 `DRM_MAJOR=226` 但次设备号模拟

---

## 后果

### 正面

- **KFD 可编译**：`chmod`/`chown`/`fchmod`/`access` 接口存在，KFD 代码零修改编译通过
- **扩展点明确**：权限检查 hook 标记了未来硬化位置，不需要重构
- **向后兼容**：现有 `Device` 构造函数增加默认参数，所有现有代码零修改
- **与 Linux 约定对齐**：render node 0666 + primary node 0666 与 Linux udev 默认一致

### 负面

- **无真实安全隔离**：单用户环境下权限模型仅是"装饰"，不能阻止任何访问（可接受，UsrLinuxEmu 定位是开发环境）
- **模式位语义不完整**：没有 sticky bit、setuid、ACL 等高级权限特性（暂不需要）
- **registry key 扁平化**：用 `"dri/renderD128"` 字符串做 key，非真实目录树（简化设计，未来如需真实目录层级需重构）

### 风险

| 风险 | 概率 | 缓解 |
|------|------|------|
| `chown` 强校验在 1.4 暴露问题 | 低 | 记录在 `kfd-portability-report.md`，标注"不强校验调用者身份" |
| 扩展 `Device` 字段导致插件 ABI 断裂 | 低 | kernel 库本已是 SHARED，新增字段默认值保证二进制兼容 |
| registry key 策略在 Stage 2 多设备时不够用 | 中 | 记录在代码注释中"未来可重构为树形 registry" |

---

## 实施步骤（解锁 VFS-1 ~ VFS-4）

按追踪文档验收顺序：

```
VFS-1: Device 结构体扩展（决策 1）
  ├── include/kernel/device/device.h: + mode/uid/gid 字段
  ├── src/kernel/device.cpp: 构造函数 + 默认值
  └── 验证: 现有 40 测试无退化

VFS-2: 多段路径解析（决策 2）
  ├── src/kernel/vfs.cpp: open() 路径分段解析
  ├── 测试: tests/test_vfs_path_standalone（新增）
  └── 验证: 现有 40 测试 + 新测试全绿

VFS-3: 权限检查 hook（决策 3）
  ├── src/kernel/vfs.cpp: check_permission() placeholder
  └── 验证: 编译通过 + open() 行为无变化

VFS-4: chmod/chown/fchmod/access 接口（决策 4）
  ├── include/kernel/vfs.h: 方法声明
  ├── src/kernel/vfs.cpp: 方法实现
  ├── 测试: tests/test_permission_standalone（按需，基础覆盖）
  └── 验证: 编译通过 + 方法可调用
```

---

**维护者**: UsrLinuxEmu Architecture Team
**创建日期**: 2026-07-02
**对应 Change**: `openspec/changes/stage-1-2-drm-subset/`
**对应追踪**: `docs/superpowers/plans/2026-07-02-stage-1-kernel-emu-tracking.md` §Sub-stage 1.2