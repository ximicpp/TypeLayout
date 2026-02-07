# Change: Refactor to Two-Layer Signature System (Layout + Definition)

## Why

当前的三模式系统（Physical / Structural / Annotated）存在根本性的概念缺陷：

1. **Structural 是不干净的中间态** — 它保留了继承结构但丢弃了名字，既不够纯净做字节比较，又不够完整做语义诊断。它的存在缺乏理论支撑。

2. **Physical 和 Structural 的关系不明确** — Physical 是 Structural 的投影吗？不是，它是独立的扁平化逻辑。三者之间没有清晰的数学关系。

3. **Annotated 被定位为"调试模式"** — 但实际上带名字的签名才是完整的类型定义表达，应该是一等公民而非附属。

两层签名系统建立在严格的数学关系上：

```
Definition Signature ──── project() ────→ Layout Signature
```

- **Layout Signature（布局签名）**：纯字节偏移映射，回答"每个偏移处存放什么原始类型"
- **Definition Signature（定义签名）**：完整类型定义树，回答"这个类型的结构是什么"

布局签名是定义签名的**投影（projection）**：丢弃名字、展平继承、按偏移排序。这是多对一映射：不同的定义可以投影到同一布局。

## What Changes

- **BREAKING**: 删除 `SignatureMode::Physical`、`SignatureMode::Structural`、`SignatureMode::Annotated` 三个枚举值
- **BREAKING**: 删除所有旧 API：`get_structural_signature`、`get_annotated_signature`、`get_physical_signature`、`signatures_match`、`physical_signatures_match`、`hashes_match`、`physical_hashes_match` 等
- **BREAKING**: 删除所有旧 Concepts：`LayoutCompatible`、`PhysicalLayoutCompatible`、`LayoutMatch`、`LayoutHashMatch`、`LayoutHashCompatible`、`PhysicalHashCompatible`
- **BREAKING**: 删除旧宏：`TYPELAYOUT_BIND`、`TYPELAYOUT_ASSERT_PHYSICAL_COMPATIBLE`、`TYPELAYOUT_BIND_PHYSICAL`
- 新增 `SignatureMode::Layout` 和 `SignatureMode::Definition`
- 新增完整的两层 API：`get_layout_signature`、`get_definition_signature` 及全套配套函数
- 新增两层 Concepts：`LayoutCompatible`、`DefinitionCompatible` 等
- 签名格式统一使用 `record` 前缀（不再区分 `struct`/`class`）
- Definition 签名使用 `~base<Name>` / `~vbase<Name>` 格式（含基类名）
- Definition 签名包含字段名 `[name]`，但不包含外层类型名
- Definition 签名保留 `polymorphic` 标记

## Impact

- Affected specs: `signature`
- Affected code: 所有 `core/` 头文件、所有测试文件、所有示例文件、README、设计文档
- 这是一次**完整的 API 破坏性重构**，所有用户代码必须迁移
