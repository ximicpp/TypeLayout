# Change: 重构为 Core/Util 分层架构

## Why

当前代码结构将"布局签名"和"序列化检查"混合在同一层级，但它们有本质区别：

1. **布局签名是库核心价值**：提供类型内存布局的完整描述，是 Boost 入库的核心竞争力
2. **序列化检查是应用层功能**：基于布局签名构建的一个具体用例，展示核心能力的实用性
3. **Boost 评审关注点**：评审者更关注独特的、可复用的基础设施，而非领域特定的策略

需要明确定位：**Layout Signature 是核心，Serialization Check 是基于核心的实用工具**

## What Changes

### 目录结构重组

```
include/boost/typelayout/
├── core/                           # 核心（入库重点）
│   ├── layout_signature.hpp        # 布局签名生成
│   ├── type_descriptor.hpp         # 类型描述符
│   ├── compile_string.hpp          # 编译时字符串
│   ├── hash.hpp                    # 哈希算法
│   └── config.hpp                  # 配置
│
├── util/                           # 实用工具（展示应用）
│   ├── serialization_check.hpp     # 序列化安全检查
│   └── platform_set.hpp            # 平台集定义
│
├── typelayout.hpp                  # 主头文件（包含 core）
└── typelayout_util.hpp             # 实用工具头文件（包含 util）
```

### 文档叙事调整

**新定位**：
> Boost.TypeLayout 提供编译期类型布局签名，保证相同签名意味着二进制兼容。
> 
> 库同时提供 `util/` 模块，展示如何基于布局签名构建领域特定的安全检查。

### API 分类

**Core API（核心）**：
- `get_layout_signature<T>()`
- `get_layout_hash<T>()`
- `get_layout_verification<T>()`
- `signatures_match<T1, T2>()`
- `LayoutCompatible<T, U>` concept
- `LayoutMatch<T, Sig>` concept
- `LayoutHashMatch<T, Hash>` concept

**Util API（实用工具）**：
- `is_serializable_v<T, P>`
- `serialization_status<T, P>()`
- `has_bitfields<T>()`
- `Serializable<T>` concept
- `ZeroCopyTransmittable<T>` concept

## Impact

- **Affected specs**: `signature`, `portability`, `use-cases`, `documentation`
- **Affected code**: 
  - `include/boost/typelayout/` 目录结构
  - 所有 `#include` 路径
  - 文档叙事和示例
- **Breaking changes**: 头文件路径变更（提供兼容层）
