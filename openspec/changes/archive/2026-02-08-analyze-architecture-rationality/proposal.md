# Change: Analyze Architecture Rationality

## Why
TypeLayout 的技术架构包含多个关键设计决策：两层签名系统、两阶段管线、header-only + consteval、
FixedString 编译时字符串。需要分析每个决策的必要性和合理性，论证"为什么这样设计"而非"为什么不那样设计"。

## What Changes
- 分析两层签名 (Layout + Definition) 的必要性 — 为何不是一层或三层？
- 分析两阶段管线 (Export + Check) 的合理性 — 为何分离 P2996 依赖？
- 分析 header-only + consteval 的工程决策
- 分析 FixedString<N> 方案的性能边界和替代方案
- 将架构决策的合理性论证记录到 signature spec

## Impact
- Affected specs: signature
- Affected code: 无代码变更，纯分析性质
