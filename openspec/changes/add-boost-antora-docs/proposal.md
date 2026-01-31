# Change: 创建 Boost Antora 风格文档套件

## Why
当前项目文档是简单的 Markdown 格式，缺乏完整性和专业性。为了符合 Boost 库标准并提供优质用户体验，需要创建完整的 Antora 风格文档套件（类似 Boost.JSON、Boost.URL 等现代 Boost 库）。

现有文档问题：
- 格式不统一（Markdown 而非 Asciidoc）
- 缺少结构化导航
- 缺少设计原理 (Rationale) 文档
- 缺少配置选项文档
- 缺少完整的示例代码

## What Changes
- **新增** Antora 文档站点配置 (`antora.yml`, `antora-playbook.yml`)
- **新增** 完整的 Asciidoc 文档结构:
  - `modules/ROOT/pages/` - 主页和概述
  - `modules/ROOT/pages/user-guide/` - 用户指南
  - `modules/ROOT/pages/reference/` - API 参考
  - `modules/ROOT/pages/design/` - 设计原理
  - `modules/ROOT/pages/config/` - 配置选项
  - `modules/ROOT/examples/` - 示例代码
- **修改** 现有 Markdown 文档迁移至 Asciidoc 格式
- **新增** 文档构建脚本

## Impact
- Affected specs: documentation (新建)
- Affected code: `doc/` 目录重构
- 不影响库的 API 和实现

## Scope
- 文档类型: 完整 Boost 文档套件
- 格式: Asciidoc (Antora)
- 风格: 现代 Boost 文档（参考 Boost.JSON）

## Success Criteria
1. Antora 可成功构建文档站点
2. 包含完整的用户指南、API 参考、设计原理
3. 所有示例代码可编译运行
4. 符合 Boost 文档规范
