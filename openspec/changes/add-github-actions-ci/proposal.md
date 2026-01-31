# Change: 添加 GitHub Actions CI/CD 流水线

## Why
Boost 社区要求库具备持续集成能力以验证多平台兼容性。当前 TypeLayout 缺少 CI/CD 配置，无法自动化验证代码质量和测试覆盖，这是**阻塞 Boost 提交的关键问题**。

## What Changes
1. 创建 GitHub Actions 工作流配置
2. 配置多平台构建矩阵
3. 集成测试运行和报告
4. 添加代码质量检查

## 详细设计

### 1. 工作流文件结构

```
.github/
└── workflows/
    ├── ci.yml              # 主 CI 流水线
    ├── docs.yml            # 文档构建检查
    └── release.yml         # 发布流程 (可选)
```

### 2. CI 流水线配置 (`ci.yml`)

```yaml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  build:
    strategy:
      matrix:
        include:
          # Bloomberg Clang P2996 (主要目标)
          - os: ubuntu-latest
            compiler: clang-p2996
            name: "Ubuntu / Clang P2996"
          # 文档构建验证
          - os: ubuntu-latest
            compiler: docs-only
            name: "Documentation Build"

    runs-on: ${{ matrix.os }}
    name: ${{ matrix.name }}

    steps:
      - uses: actions/checkout@v4

      - name: Setup Bloomberg Clang P2996
        if: matrix.compiler == 'clang-p2996'
        run: |
          # 下载并配置 Bloomberg Clang P2996 fork
          # (具体步骤取决于可用的预编译版本)

      - name: Build and Test
        if: matrix.compiler == 'clang-p2996'
        run: |
          mkdir build && cd build
          cmake .. -DCMAKE_CXX_COMPILER=clang++ \
                   -DCMAKE_CXX_FLAGS="-std=c++26 -freflection"
          cmake --build .
          ctest --output-on-failure

      - name: Build Documentation
        if: matrix.compiler == 'docs-only'
        run: |
          cd doc
          npm install
          npm run build
```

### 3. 构建矩阵

| 平台 | 编译器 | 用途 |
|------|--------|------|
| Ubuntu | Bloomberg Clang P2996 | 主要功能测试 |
| Ubuntu | Node.js | 文档构建验证 |

> **注**: 由于 P2996 仅 Bloomberg Clang 支持，Windows/macOS 矩阵暂不可用。待标准化后扩展。

### 4. 触发条件

| 事件 | 分支 | 动作 |
|------|------|------|
| Push | main, develop | 完整 CI |
| PR | main | 完整 CI |
| Release | tags/v* | 发布流程 |

### 5. 状态徽章

添加到 README.md:
```markdown
[![CI](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml)
```

## Impact
- 满足 Boost 库 CI 要求
- 自动化质量验证
- 提供构建状态可视化
