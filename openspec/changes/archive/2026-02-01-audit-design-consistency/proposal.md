# Change: 审计代码与设计目标的一致性

## Why

在实现分层签名 (Layered Signatures) 功能后，项目中现在存在**两套**序列化检查机制：

1. **旧 API** (`portability.hpp`): `is_trivially_serializable<T>()` - 无平台参数，隐式使用当前平台
2. **新 API** (`serialization_signature.hpp`): `is_serializable_v<T, PlatformSet>` - 必须指定平台集

这两套 API 在语义和实现上有重叠但不完全一致，可能导致用户混淆。需要审计并明确它们的定位。

## 发现的问题

### 问题 1: API 语义重叠

| API | 文件 | 平台参数 | 检查内容 |
|-----|------|---------|---------|
| `is_trivially_serializable<T>()` | portability.hpp | 隐式当前 | 指针、引用、位域 |
| `is_serializable_v<T, P>` | serialization_signature.hpp | 必须指定 | 指针、引用、多态、long |

**冲突点**: 
- 两者都声称检查"序列化安全性"
- 检查项不完全相同（旧API检查位域，新API检查long）
- 用户不知道该用哪个

### 问题 2: 设计目标不一致

根据 `openspec/project.md` 的核心理念：
> **核心保证**: `Identical signature ⟺ Identical memory layout`

但新的序列化签名 `[64-le]serial` 并不是完整的布局签名，只是一个状态标记。这与"签名"的语义不一致。

### 问题 3: 文档描述不统一

| 位置 | 描述 |
|------|------|
| README Layer 2 | "Serialization Compatibility" |
| portability.hpp | "is_trivially_serializable" |
| project.md | 只提到 `is_portable` (旧名称) |

### 问题 4: project.md 过时

`project.md` 仍在使用 `is_portable<T>()` 描述，没有更新为 `is_trivially_serializable<T>()` 或新的分层签名 API。

### 问题 5: specs 目录中存在冗余/过时 specs

- `openspec/specs/analysis/` - 分析性 spec，不是功能 spec
- `openspec/specs/design-issues/` - 问题跟踪，不是功能 spec
- `openspec/specs/use-cases/` - 用例描述，不是功能 spec

这些应该是设计文档，而不是 specs。

## What Changes

### 1. 统一序列化检查 API（决策：选项 A）

**✅ 采用选项 A**: 废弃旧 API，统一到新 API
- 标记 `is_trivially_serializable<T>()` 为 `[[deprecated]]`
- 用户迁移到 `is_serializable_v<T, PlatformSet::current()>`
- 强制用户明确指定平台集

### 2. 更新 project.md

- 更新 API 名称 (`is_portable` → `is_trivially_serializable`)
- 添加分层签名 API 说明
- 明确两层架构

### 3. 清理 specs 目录

- 将分析性 specs 移到 `docs/` 目录
- 只保留描述功能行为的 specs

### 4. 统一序列化检查逻辑

确保新旧 API 检查以下相同项目：
- [ ] 非平凡可复制类型 (not trivially copyable)
- [ ] 指针成员 (pointer)
- [ ] 引用成员 (reference)
- [ ] 多态类型 (polymorphic)
- [ ] 位域 (bit-fields)
- [ ] 平台依赖类型 (long)

## Impact

- **Affected specs**: portability, signature
- **Affected code**: `portability.hpp`, `serialization_signature.hpp`
- **Affected docs**: `project.md`, `README.md`, `docs/architecture.md`
- **Breaking changes**: 可能需要废弃 `is_trivially_serializable<T>()`
