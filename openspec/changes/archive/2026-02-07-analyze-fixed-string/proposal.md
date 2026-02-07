# Change: 分析 FixedString<N> 实现的正确性和效率

## Why
`FixedString<N>` 是整个签名系统的基础设施，所有签名字符串的构造、拼接、比较都依赖它。
需要系统性审查其正确性（边界条件、UB风险）和效率（constexpr 步数消耗），
并提出可行的优化方案。

## What Changes
- 分析每个成员函数的正确性
- 评估 constexpr 步数效率
- 提出优化建议（若有）并实施

## Impact
- Affected specs: signature（FixedString 是签名生成的核心依赖）
- Affected code: `include/boost/typelayout/core/fwd.hpp`
