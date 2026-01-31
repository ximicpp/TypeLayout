## Context

Boost.TypeLayout 是一个 C++26 库，需要符合 Boost 库文档标准。现代 Boost 库（如 Boost.JSON、Boost.URL）采用 Antora 作为文档生成工具，使用 Asciidoc 格式。

**参考项目**:
- [Boost.JSON 文档](https://www.boost.org/doc/libs/develop/libs/json/doc/html/index.html)
- [Boost.URL 文档](https://www.boost.org/doc/libs/develop/libs/url/doc/html/index.html)

## Goals

1. 创建符合 Boost 标准的完整文档套件
2. 采用 Antora + Asciidoc 现代文档方案
3. 提供清晰的用户指南和 API 参考
4. 包含设计原理说明（Rationale）
5. 所有示例代码可直接编译运行

## Non-Goals

1. 不修改库的 API 或实现
2. 不支持传统 Boostbook/Quickbook 格式
3. 不自动生成 API 文档（Doxygen 风格）

## Decisions

### 1. 文档格式选择: Asciidoc

**选择**: Asciidoc (via Antora)

**原因**:
- Boost 官方推荐的现代文档格式
- 比 Markdown 更强大（表格、警告框、交叉引用）
- Antora 提供优秀的多版本支持和导航
- 与其他现代 Boost 库保持一致

**备选方案**:
- Markdown: 太简单，不适合复杂技术文档
- Boostbook: 过时，学习曲线陡峭
- reStructuredText: 不是 Boost 标准

### 2. 目录结构: Antora 模块化

**选择**: 单模块 (`modules/ROOT/`) 结构

**原因**:
- TypeLayout 是单一库，无需多模块
- 简化维护和导航
- 符合大多数 Boost 库结构

**目录布局**:
```
doc/
├── antora.yml                    # 站点配置
├── antora-playbook.yml           # Playbook
└── modules/
    └── ROOT/
        ├── nav.adoc              # 导航
        ├── pages/                # 文档页面
        │   ├── index.adoc        # 主页
        │   ├── overview.adoc     # 概述
        │   ├── quickstart.adoc   # 快速入门
        │   ├── user-guide/       # 用户指南
        │   ├── reference/        # API 参考
        │   ├── design/           # 设计原理
        │   └── config/           # 配置选项
        └── examples/             # 示例代码
```

### 3. 用户指南组织: 按功能分章

**章节结构**:
1. 布局签名 (Layout Signatures) - 核心概念
2. 类型支持 (Type Support) - 支持的所有类型
3. 可移植性检查 (Portability) - 跨平台兼容
4. 模板约束 (Concepts) - C++20 Concepts 使用
5. 哈希验证 (Hash Verification) - 双哈希系统
6. 继承支持 (Inheritance) - 类继承处理
7. 位域支持 (Bitfields) - 位域特殊处理

### 4. API 参考组织: 按类别分组

**分组结构**:
1. 核心函数 (Core Functions)
2. Concepts (模板约束)
3. 宏 (Macros)
4. 工具类 (Utility Classes)
5. 类型签名 (Type Signatures)

### 5. 示例代码: 可编译的独立文件

**策略**:
- 每个示例是独立的 `.cpp` 文件
- 使用 Asciidoc `include::` 指令嵌入
- 添加到 CMake 测试确保可编译

## Risks / Trade-offs

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| Antora 学习曲线 | 维护成本增加 | 提供文档贡献指南 |
| 文档与代码不同步 | 用户困惑 | 示例代码加入 CI 测试 |
| 中英文混合 | 阅读体验不一致 | 全英文文档，中文可作为单独翻译 |

## Migration Plan

1. 保留现有 Markdown 文档作为备份
2. 逐步迁移内容到 Asciidoc
3. 验证所有功能都有文档覆盖
4. 最终删除或存档旧 Markdown 文件

## Open Questions

1. 是否需要支持多语言（中文翻译）？
2. 是否需要与 Boost 官网集成？
3. 示例代码是否需要支持 32 位平台？
