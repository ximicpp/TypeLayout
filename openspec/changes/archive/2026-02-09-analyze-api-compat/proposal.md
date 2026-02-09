# Change: API 兼容检查场景与两层签名的关系分析

## Why
库的公共 API 中涉及的结构体（参数类型、返回类型）在版本升级时可能发生变更。API 兼容检查需要在语义层面验证结构体是否保持一致——不仅是字节布局，还包括字段名、继承层次、命名空间等。需要分析 Definition 层为什么是 API 兼容检查的核心工具，以及两层签名如何区分 ABI 兼容与 API 兼容。

## What Changes
- 新增 API 兼容检查场景的深度分析文档
- 分析内容包括：
  - Definition 层在 API 兼容检查中的核心角色
  - ABI 兼容（Layout 层）vs API 兼容（Definition 层）的精确区分
  - 继承层次变更的检测（基类重构、虚继承变更）
  - TypeLayout 的结构分析 vs 名义分析设计选择的影响
  - 典型案例：库升级中保持 ABI 但破坏 API 的场景

## Impact
- Affected specs: signature
- Affected code: 无代码变更
