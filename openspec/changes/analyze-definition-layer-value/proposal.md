# Change: 分析 Definition 层签名的价值与合理性

## Why
TypeLayout 的双层签名系统是核心架构决策。Layout 层的价值显而易见（字节布局一致性验证），
但 Definition 层的**增量价值**需要严格论证：它究竟解决了哪些 Layout 层无法解决的问题？
这些问题是否真实存在？代价（额外复杂度）是否值得？

## What Changes
- 系统性列举 Definition 层相对于 Layout 层的**差异信息**（字段名、继承层次、枚举限定名）
- 逐项分析每项差异信息的实际价值与使用场景
- 评估"没有 Definition 层"时的替代方案成本
- 识别 Definition 层的**反面论点**（过度设计、增加用户认知负担）
- 给出最终判断：保留、简化还是移除

## Impact
- Affected specs: signature
- Affected code: `signature_detail.hpp` (Definition 引擎部分 Part 2)
- 决定双层架构是否值得在 Boost 评审中作为核心卖点
