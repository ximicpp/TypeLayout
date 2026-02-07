# Change: Analyze TypeLayout design and implementation against core value

## Why

TypeLayout 经历了多次重构（仓库精简、双层签名系统、签名正确性修复），需要一次系统性审计来回答核心问题：**当前的设计和实现是否实现了库的核心价值？**

核心价值声明：
> `Identical signature ⟺ Identical memory layout`（相同签名等价于相同内存布局）

## What Changes

这是一个**纯分析提案**，不涉及代码变更。产出为分析报告，可能衍生后续修复提案。

分析维度：
- **核心保证的数学正确性**：签名的双射性（bijection）是否成立？
- **双层系统的完备性**：Layout/Definition 两层是否覆盖所有必要场景？
- **投射关系的可靠性**：`definition_match ⟹ layout_match` 是否在所有类型上成立？
- **API 与声称功能的差距**：project.md/README 声称的功能 vs 实际实现
- **边界情况**：空类型、位域、虚继承、模板类型、嵌套命名空间等
- **实用性评估**：库在其目标场景（IPC、共享内存、跨平台验证）中是否真正可用？

## Impact

- Affected specs: `signature`
- Affected code: 所有 `core/` 头文件（分析对象，不修改）
- 产出: 分析报告（本 proposal 内），可能衍生 0~N 个后续修复提案