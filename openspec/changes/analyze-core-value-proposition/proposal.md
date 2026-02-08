# Change: Analyze Core Value Proposition (V1/V2/V3)

## Why
Boost.TypeLayout 的三大核心价值承诺 (V1 可靠性、V2 精确性、V3 投影关系) 是整个方案的理论基石。
需要系统性分析这些承诺的数学正确性、边界条件、以及"结构分析 vs 名义分析"设计哲学的利弊权衡，
确保方案在理论层面站得住脚。

## What Changes
- 分析 V1 (`layout_sig match ⟹ memcmp-compatible`) 的正确性边界和反例
- 分析 V2 (`def_sig match ⟹ identical structure`) 的区分力和充分性
- 分析 V3 (`def_match ⟹ layout_match`) 的代数性质和实际价值
- 评估"结构分析"vs"名义分析"的设计哲学
- 与竞品 (protobuf, FlatBuffers, pahole, static_assert) 的差异化定位
- 将分析结论正式记录到 signature spec

## Impact
- Affected specs: signature
- Affected code: 无代码变更，纯分析性质
