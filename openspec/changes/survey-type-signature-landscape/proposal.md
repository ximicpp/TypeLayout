# Change: 调研类型签名领域，分析 TypeLayout 方案的独特价值

## Why
TypeLayout 不是第一个尝试在 C++ 中提供"类型身份"信息的方案。在提交 Boost 评审前，
需要系统性地调研已有的类型签名/类型身份方案，明确 TypeLayout 在这个领域的定位：
它解决了什么现有方案解决不了的问题？它的独特技术路线（P2996 编译时反射 + 双层签名）
相比其他路线有什么结构性优势？

## What Changes
- 系统调研 C++ 生态中的类型签名/身份方案（RTTI、Boost.TypeIndex、ABI mangling 等）
- 调研其他语言的类型签名方案（Rust、Java、Protocol Buffers）
- 对比分析 TypeLayout 的技术差异化
- 识别 TypeLayout 的"不可替代区间"
- 评估 Boost 评审者可能的质疑点

## Impact
- Affected specs: signature
- 为 Boost 评审提供"为什么现有方案不够用"的论据
- 明确 TypeLayout 的市场定位
