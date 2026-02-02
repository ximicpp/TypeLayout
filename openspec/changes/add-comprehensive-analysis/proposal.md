## Why

当前仓库已完成 Core/Util 分层架构重构，但缺乏一份完整的、面向不同读者群体的功能与原理分析文档。需要一份系统化的分析，涵盖：
1. 面向新用户的功能概述与原理介绍
2. 面向 Boost 审核者/贡献者的深入技术架构分析
3. 与竞品/替代方案的对比分析
4. 性能分析与基准测试文档

## What Changes

- 创建 `doc/modules/ROOT/pages/design/architecture-analysis.adoc`: 完整的架构分析文档
- 创建 `doc/modules/ROOT/pages/design/comparison.adoc`: 竞品对比分析
- 创建 `doc/modules/ROOT/pages/design/performance.adoc`: 性能分析与基准测试
- 更新 `doc/modules/ROOT/nav.adoc`: 添加新文档的导航链接
- 更新 OpenSpec `analysis` 规范

## Impact

- Affected specs: `analysis`, `documentation`
- Affected code: `doc/modules/ROOT/pages/design/`
- 输出格式: AsciiDoc (Antora)
