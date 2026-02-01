# Change: Refactor Portability API Naming

## Why

当前的 `is_portable<T>()` 名称语义不明确：
- "Portable" 可能被理解为"跨平台可编译"
- 实际含义是"可以通过 memcpy 序列化并跨进程传输"
- 需要更精确的命名来表达这个概念

同时，需要在文档中明确 TypeLayout 的分层架构：
- **核心层**：布局签名系统（纯布局验证，不做语义判断）
- **辅助层**：序列化检查工具（帮助用户判断类型是否适合特定场景）

## What Changes

1. **重命名 API**：
   - `is_portable<T>()` → `is_trivially_serializable<T>()`
   - `is_portable_v<T>` → `is_trivially_serializable_v<T>`
   - `Portable<T>` concept → `TriviallySerializable<T>`

2. **保留向后兼容**：
   - 保留 `is_portable` 作为 deprecated 别名
   - 添加编译警告提示迁移

3. **添加架构文档**：
   - 在 README 或专门文档中说明分层设计
   - 明确布局签名 vs 序列化检查的职责边界

## Impact

- Affected specs: `specs/portability/` (if exists)
- Affected code:
  - `include/boost/typelayout/portability.hpp`
  - `include/boost/typelayout/concepts.hpp`
  - `include/boost/typelayout/fwd.hpp`
  - `docs/` or `README.md`
- **BREAKING**: 用户代码使用 `is_portable` 需要迁移（提供 deprecated 别名过渡）
