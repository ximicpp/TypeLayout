# Change: Analyze Definition Signature Correctness and Completeness

## Why
Definition 签名（第二层）是 TypeLayout 的核心差异化能力——它在 Layout 的字节身份之上增加了结构身份（字段名、继承树、限定名）。
但当前缺乏系统性审计来验证：签名格式是否正确地区分了所有应该区分的情况？是否遗漏了某些应该编码的结构信息？

## What Changes
本次为纯分析性提案，不修改代码。产出为：
- 完整的 Definition 签名正确性/完备性分析报告（`design.md`）
- 发现的问题和改进建议
- 如有需要修复的问题，将作为后续提案的输入

## Impact
- Affected specs: signature
- Affected code: `signature_detail.hpp` (Definition 引擎), `signature.hpp` (API)
