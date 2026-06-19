# ADR-009: 采用单例模式实现核心服务

**状态**: ✅ 已接受 (Accepted)

**日期**: 2026-01

## 背景

某些核心服务（如 VFS、ServiceRegistry）在系统中应该只有一个实例，需要全局访问。

## 决策

对核心服务使用单例模式。

## 理由

1. **全局唯一**: 保证服务只有一个实例
2. **全局访问**: 任何地方都可以访问服务
3. **延迟初始化**: 可以在首次使用时初始化
4. **资源管理**: 易于管理全局资源

## 实现方式

```cpp
class ServiceRegistry {
public:
    static ServiceRegistry& instance() {
        static ServiceRegistry inst;
        return inst;
    }
private:
    ServiceRegistry() = default;
    ServiceRegistry(const ServiceRegistry&) = delete;
    ServiceRegistry& operator=(const ServiceRegistry&) = delete;
};
```

## 后果

- ✅ 全局唯一性保证
- ✅ 方便访问
- ✅ 资源管理清晰
- ⚠️ 增加全局状态（可能影响测试）
- ⚠️ 需要考虑线程安全

---

**维护者**: UsrLinuxEmu Architecture Team
**最后更新**: 2026-01