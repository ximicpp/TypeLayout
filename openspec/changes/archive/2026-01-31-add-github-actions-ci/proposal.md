# Change: 添加 GitHub Actions CI/CD 流水线 (Docker 镜像方案)

## Why
Boost 社区要求库具备持续集成能力以验证多平台兼容性。当前 TypeLayout 缺少 CI/CD 配置，无法自动化验证代码质量和测试覆盖，这是**阻塞 Boost 提交的关键问题**。

**关键挑战**: Bloomberg Clang P2996 分支不是标准发行版，GitHub Actions 官方 runner 不包含此编译器。需通过自建 Docker 镜像解决。

## What Changes
1. 创建包含 Bloomberg Clang P2996 的 Docker 镜像
2. 推送镜像到 GitHub Container Registry (ghcr.io)
3. 创建基于 Docker 的 GitHub Actions 工作流
4. 配置文档构建检查

## 详细设计

### 1. 目录结构

```
.github/
├── docker/
│   └── Dockerfile.p2996      # Bloomberg Clang P2996 镜像
└── workflows/
    ├── docker-build.yml      # 构建并推送 Docker 镜像
    ├── ci.yml                # 主 CI 流水线
    └── docs.yml              # 文档构建检查
```

### 2. Docker 镜像构建 (`Dockerfile.p2996`)

```dockerfile
# 基于 Ubuntu 22.04 构建 Bloomberg Clang P2996
FROM ubuntu:22.04 AS builder

# 避免交互式提示
ENV DEBIAN_FRONTEND=noninteractive

# 安装构建依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    python3 \
    wget \
    lsb-release \
    software-properties-common \
    && rm -rf /var/lib/apt/lists/*

# 克隆 Bloomberg Clang P2996 分支
WORKDIR /opt
RUN git clone --depth 1 -b p2996 \
    https://github.com/bloomberg/clang-p2996.git llvm-project

# 构建 Clang
WORKDIR /opt/llvm-project
RUN cmake -S llvm -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" \
    -DLLVM_TARGETS_TO_BUILD="X86" \
    -DCMAKE_INSTALL_PREFIX=/opt/clang-p2996

RUN cmake --build build --target install -j$(nproc)

# 最终精简镜像
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    git \
    libstdc++-12-dev \
    && rm -rf /var/lib/apt/lists/*

# 复制编译好的 Clang
COPY --from=builder /opt/clang-p2996 /opt/clang-p2996

# 设置环境变量
ENV PATH="/opt/clang-p2996/bin:${PATH}"
ENV CC=/opt/clang-p2996/bin/clang
ENV CXX=/opt/clang-p2996/bin/clang++

# 验证安装
RUN clang++ --version

LABEL org.opencontainers.image.source=https://github.com/ximicpp/TypeLayout
LABEL org.opencontainers.image.description="Bloomberg Clang P2996 for TypeLayout CI"
```

### 3. Docker 镜像构建工作流 (`docker-build.yml`)

```yaml
name: Build Docker Image

on:
  # 手动触发或定期更新
  workflow_dispatch:
  schedule:
    - cron: '0 0 * * 0'  # 每周日构建

env:
  REGISTRY: ghcr.io
  IMAGE_NAME: ${{ github.repository }}/clang-p2996

jobs:
  build:
    runs-on: ubuntu-latest
    permissions:
      contents: read
      packages: write

    steps:
      - uses: actions/checkout@v4

      - name: Log in to Container Registry
        uses: docker/login-action@v3
        with:
          registry: ${{ env.REGISTRY }}
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}

      - name: Build and Push
        uses: docker/build-push-action@v5
        with:
          context: .github/docker
          file: .github/docker/Dockerfile.p2996
          push: true
          tags: |
            ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}:latest
            ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}:${{ github.sha }}
          cache-from: type=gha
          cache-to: type=gha,mode=max
```

### 4. 主 CI 流水线 (`ci.yml`)

```yaml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

env:
  REGISTRY: ghcr.io
  IMAGE_NAME: ghcr.io/ximicpp/typelayout/clang-p2996:latest

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    container:
      image: ghcr.io/ximicpp/typelayout/clang-p2996:latest
      credentials:
        username: ${{ github.actor }}
        password: ${{ secrets.GITHUB_TOKEN }}

    steps:
      - uses: actions/checkout@v4

      - name: Verify Compiler
        run: |
          clang++ --version
          echo "P2996 reflection support verified"

      - name: Configure
        run: |
          cmake -S . -B build -G Ninja \
            -DCMAKE_CXX_COMPILER=clang++ \
            -DCMAKE_CXX_FLAGS="-std=c++26 -freflection" \
            -DCMAKE_BUILD_TYPE=Release

      - name: Build
        run: cmake --build build

      - name: Test
        run: |
          cd build
          ctest --output-on-failure

  docs:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Setup Node.js
        uses: actions/setup-node@v4
        with:
          node-version: '20'

      - name: Build Documentation
        run: |
          cd doc
          npm install
          npm run build
```

### 5. 工作流策略

| 工作流 | 触发条件 | 用途 | 预计时间 |
|--------|----------|------|----------|
| docker-build.yml | 手动/每周 | 构建编译器镜像 | 1-2 小时 |
| ci.yml | Push/PR | 编译+测试 | 5-10 分钟 |
| docs.yml | Push/PR | 文档验证 | 2-3 分钟 |

### 6. 镜像管理策略

- **基础镜像**: 每周自动重建以获取安全更新
- **版本标签**: 使用 `latest` + commit SHA 双标签
- **缓存优化**: 利用 GitHub Actions 缓存加速后续构建
- **大小优化**: 使用多阶段构建减小最终镜像体积

### 7. 状态徽章

添加到 README.md:
```markdown
[![CI](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/TypeLayout/actions/workflows/ci.yml)
[![Docker](https://github.com/ximicpp/TypeLayout/actions/workflows/docker-build.yml/badge.svg)](https://github.com/ximicpp/TypeLayout/actions/workflows/docker-build.yml)
```

## Impact
- **解决 P2996 环境问题**: 自建 Docker 镜像提供稳定的编译环境
- **满足 Boost CI 要求**: 自动化构建和测试验证
- **降低维护成本**: 镜像复用避免每次 CI 编译 LLVM
- **可扩展性**: 未来可添加更多测试矩阵

## 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| Docker 构建耗时长 | 首次 1-2 小时 | 周期性构建 + 缓存复用 |
| Bloomberg 仓库变更 | 镜像构建失败 | 固定 commit 或 tag |
| 镜像体积大 | 存储成本 | 多阶段构建优化 |