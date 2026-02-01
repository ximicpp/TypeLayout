# Change: 分层签名 - Layout 兼容性与序列化兼容性

## Why

当前 TypeLayout 只提供单一的布局签名（Layout Signature），用于检测内存布局是否一致。但在实际应用中，存在两种不同层次的兼容性需求：

1. **Layout 兼容性**：内存布局一致，可用于同进程或同平台的类型检查
2. **序列化兼容性**：可通过 memcpy 跨进程/跨机器安全传输数据，是更严格的兼容性要求

序列化兼容性需要在 Layout 兼容的基础上，额外排除不可序列化的类型（指针、引用、多态类型等），并支持用户指定目标平台集（如 64-le）。

## What Changes

### 新增 API

1. **`get_serialization_signature<T, PlatformSet>()`** - 获取指定平台集的序列化兼容性签名
2. **`is_serializable<T, PlatformSet>`** - 编译期检查类型在指定平台集上是否可序列化
3. **`PlatformSet`** - **必须指定**的平台约束（字节序、位宽等）
4. **`serialization_blocker<T>`** - 诊断阻止序列化的原因

### 两层架构

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 2: Serialization Compatibility (序列化兼容性)         │
│  ─────────────────────────────────────────────────────────  │
│  • 必须指定平台集 P（如 64-le）                              │
│  • Layout 兼容 + 无指针 + 无多态 + 平台约束                  │
│  • 用于：跨进程/跨机器 memcpy 传输                          │
├─────────────────────────────────────────────────────────────┤
│  Layer 1: Layout Compatibility (布局兼容性)                  │
│  ─────────────────────────────────────────────────────────  │
│  • 隐式当前平台                                              │
│  • 内存布局完全一致（size, alignment, field offsets）       │
│  • 用于：同进程类型检查、IPC 共享内存（同构机器）           │
└─────────────────────────────────────────────────────────────┘
```

### 核心设计理念

**Layer 1 (Layout)**: 检查内存布局是否一致，不关心是否可 memcpy 到其他机器
**Layer 2 (Serialization)**: 在 Layout 兼容基础上，检查是否可安全地 memcpy 到**指定平台集**的机器

```
序列化兼容性(P) ⊂ Layout 兼容性

其中 P = 用户指定的平台集合（如 64-le）
```

### API 设计

| 层次 | API | 平台参数 |
|-----|-----|---------|
| Layer 1 | `get_layout_signature<T>()` | 隐式当前平台 |
| Layer 1 | `check_layout_compatible<T, U>()` | 隐式当前平台 |
| Layer 2 | `get_serialization_signature<T, P>()` | **必须指定** |
| Layer 2 | `is_serializable<T, P>` | **必须指定** |

### 序列化兼容性要求

在 Layout 兼容的基础上，类型必须满足：

1. **Trivially Copyable** - 可通过 memcpy 复制
2. **无指针/引用成员** - 指针值跨进程无意义
3. **无多态类型** - vtable 指针不可序列化
4. **平台约束匹配** - 字节序、类型大小符合目标平台集

### 序列化签名格式

```
[platform-set]struct[s:N,a:M,serial]{fields...}
                         ^^^^^^
                         新增标记表示可序列化

// 不可序列化类型示例（编译期错误或特殊标记）
[64-le]struct[s:24,a:8,!serial:ptr]{...}  // 包含指针
[64-le]class[s:16,a:8,!serial:poly]{...}  // 多态类型
```

## Impact

- **Affected specs**: signature（新增 capability）
- **Affected code**: 
  - `include/boost/typelayout/signature.hpp` - 新增序列化签名 API
  - `include/boost/typelayout/detail/serialization_traits.hpp` - 新增
  - `include/boost/typelayout/detail/type_signature.hpp` - 添加序列化标记
- **Breaking changes**: 无，纯新增功能
