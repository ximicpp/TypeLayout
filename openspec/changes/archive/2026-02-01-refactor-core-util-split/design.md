# Design: Core/Util 分层架构

## Context

Boost.TypeLayout 正在准备 Boost 入库评审。评审者通常关注：
1. 库是否解决了独特的问题
2. 核心功能是否足够通用和可复用
3. 是否有清晰的架构边界

当前实现将"布局签名"和"序列化检查"混在一起，模糊了核心价值。

## Goals / Non-Goals

### Goals
- 明确 Layout Signature 是库的核心价值
- 将 Serialization Check 定位为基于核心的应用示例
- 提供清晰的目录结构反映这种分层
- 保持向后兼容（通过兼容头文件）

### Non-Goals
- 不改变任何 API 的功能行为
- 不移除 Serialization Check 功能
- 不改变命名空间结构

## Decisions

### Decision 1: 两层目录结构

```
typelayout/
├── core/    # 库核心：布局签名
└── util/    # 实用工具：序列化检查等
```

**Rationale**: 
- 物理目录结构直接反映逻辑分层
- 用户一眼就能看出核心和扩展的区别
- Boost 评审时可以聚焦 `core/`

### Decision 2: 三个顶层头文件

| 头文件 | 内容 | 目标用户 |
|--------|------|----------|
| `typelayout.hpp` | 仅 core | 只需布局签名的用户 |
| `typelayout_util.hpp` | util (依赖 core) | 需要序列化检查的用户 |
| `typelayout_all.hpp` | core + util | 便捷引入 |

**Rationale**:
- 最小化默认包含
- 按需引入功能
- `typelayout.hpp` 作为主入口只包含核心

### Decision 3: 兼容层策略

旧路径（如 `detail/serialization_status.hpp`）保留为兼容头文件：

```cpp
// detail/serialization_status.hpp (兼容层)
#pragma once
#warning "This header is deprecated. Use <boost/typelayout/util/serialization_check.hpp>"
#include <boost/typelayout/util/serialization_check.hpp>
```

**Rationale**:
- 不破坏现有用户代码
- 编译时警告引导迁移
- 在 2-3 个版本后可移除

### Decision 4: 保持 `detail/` 作为真正的内部实现

重构后 `detail/` 仅包含：
- 真正的内部实现细节
- 兼容层重定向头文件

**Rationale**:
- `detail/` 命名约定表示"用户不应直接使用"
- 核心功能应该有更清晰的公开路径

## Dependency Graph

```
┌─────────────────────────────────────────────┐
│           typelayout_all.hpp                │
└─────────────────────────────────────────────┘
                    │
        ┌───────────┴───────────┐
        ▼                       ▼
┌───────────────────┐   ┌───────────────────┐
│  typelayout.hpp   │   │typelayout_util.hpp│
│     (core only)   │   │   (util + core)   │
└───────────────────┘   └───────────────────┘
        │                       │
        ▼                       ▼
┌───────────────────┐   ┌───────────────────┐
│      core/        │◄──│      util/        │
│ layout_signature  │   │serialization_check│
│ compile_string    │   │ platform_set      │
│ hash              │   │                   │
│ config            │   │                   │
└───────────────────┘   └───────────────────┘
```

## File Mapping

| 原路径 | 新路径 | 类别 |
|--------|--------|------|
| `detail/type_signature.hpp` | `core/layout_signature.hpp` | Core |
| `detail/compile_string.hpp` | `core/compile_string.hpp` | Core |
| `detail/hash.hpp` | `core/hash.hpp` | Core |
| `detail/config.hpp` | `core/config.hpp` | Core |
| `detail/reflection_helpers.hpp` | `core/reflection_helpers.hpp` | Core |
| `signature.hpp` | `core/signature.hpp` (public facade) | Core |
| `verification.hpp` | `core/verification.hpp` | Core |
| `detail/serialization_status.hpp` | `util/serialization_check.hpp` | Util |
| `detail/serialization_traits.hpp` | `util/serialization_traits.hpp` | Util |
| `portability.hpp` | `util/platform_set.hpp` | Util |
| `concepts.hpp` | Split: core concepts + util concepts | Both |

## Risks / Trade-offs

| 风险 | 缓解措施 |
|------|----------|
| 破坏现有用户代码 | 兼容层 + deprecation warnings |
| 增加维护复杂度 | 清晰的依赖图 + 文档 |
| 评审者可能认为 util 是核心 | 文档明确说明定位 |

## Migration Plan

1. **Phase 1**: 创建新目录结构，复制文件（不删除旧文件）
2. **Phase 2**: 将旧文件转为兼容层（include + warning）
3. **Phase 3**: 更新所有文档和示例
4. **Phase 4**: 下一个主版本移除兼容层

## Resolved Questions

1. **concepts.hpp 如何拆分？**
   - ✅ **决定**: 选项 B - 拆分为 `core/concepts.hpp` + `util/concepts.hpp`
   - **理由**: 保持一致性，明确哪些 concepts 属于核心，哪些属于实用工具

2. **是否保留 `detail/` 目录？**
   - ✅ **决定**: 保留 `detail/` 用于真正的内部实现
   - **理由**: 符合 Boost 惯例，区分公开 API 和内部实现

3. **fwd.hpp 放哪里？**
   - **决定**: 保留在顶层，提供所有前向声明
