# Change: 审计两层签名正确性与完备性

## Why
两层签名已基本成型，但尚未对每一条 spec 要求做"代码行级"的正确性验证，
也未系统性地检查 spec 本身是否完备（是否遗漏了实现中已有的行为）。

## What Changes
- 分析报告：逐条审计 spec ↔ 代码的一致性
- 识别 spec 遗漏和代码缺陷
- 提出具体修复方案，确认后再实施

## Impact
- Affected specs: `signature`
- Affected code: `reflection_helpers.hpp`, `type_signature.hpp`, `test_two_layer.cpp`, `spec.md`
