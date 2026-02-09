# Change: Analyze Cross-Platform Collection Compatibility Examples

## Why

Boost.TypeLayout 的跨平台兼容性检查工具链（SigExporter + CompatReporter）提供了
完整的两阶段 pipeline，但缺乏一份系统性文档来展示**批量类型集合**在多平台间的
兼容性判定逻辑——特别是基于两层签名系统和形式化理论（V1/V2/V3）的精确分析。

现有 APPLICATIONS.md 按场景维度分析两层签名的映射关系，但未从**跨平台集合**
维度给出完整示例：一组类型在 3 个平台（Linux x86_64、Windows x86_64、macOS ARM64）
上的签名差异、安全分级、兼容性矩阵和形式化推导。

## What Changes

- 新增 `CROSS_PLATFORM_COLLECTION.md` 分析文档，以真实 .sig.hpp 数据为基础：
  - §1: 跨平台类型集合概述（8 个类型 × 3 个平台 = 24 个签名对比）
  - §2: 平台差异根因分析（LP64 vs LLP64 数据模型、ABI 差异）
  - §3: 逐类型兼容性矩阵（Layout/Definition 两层分析）
  - §4: 安全分级的形式化基础（SafetyLevel 分类与签名模式匹配）
  - §5: 集合级兼容性判定定理（从类型级上升到集合级保证）
  - §6: 两阶段 pipeline 的正确性分析（Phase 1 导出 → Phase 2 检查）
  - §7: 实际兼容性报告解读（对 CompatReporter 输出的形式化映射）

## Impact

- Affected specs: `signature`, `cross-platform-compat`
- Affected code: 无代码变更，纯分析文档
- Affected files: 新增 `CROSS_PLATFORM_COLLECTION.md`
