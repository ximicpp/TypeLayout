## Why

作为 Boost 候选库，TypeLayout.Core 的完备性和正确性是入库评审的关键要素。需要系统性分析：
1. 类型覆盖完备性 — 是否覆盖所有 C++ 类型？
2. 签名正确性 — 生成的签名是否准确反映内存布局？
3. 边界情况处理 — 空结构体、位域跨存储单元、匿名成员等
4. 平台兼容性 — 32/64位、大小端、不同编译器
5. API 健壮性 — 错误处理、诊断信息

## What Changes

- 创建 `doc/modules/ROOT/pages/design/core-completeness.adoc`: 完备性与正确性分析报告
- 识别当前实现的潜在问题和改进机会
- 更新 OpenSpec `analysis` 规范

## Impact

- Affected specs: `analysis`
- Affected code: `doc/modules/ROOT/pages/design/`
- 输出格式: AsciiDoc (Antora)
