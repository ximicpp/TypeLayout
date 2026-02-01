# Change: 验证 CI/Docker 构建管道

## Why
Docker 镜像构建工作流已创建但尚未完成验证。需要确保整个 CI 管道（包括 Bloomberg Clang P2996 Docker 镜像构建、推送至 GHCR、以及使用该镜像运行测试）能够成功运行。

## What Changes
- 验证 Docker 镜像构建工作流 (`docker-build.yml`) 成功完成
- 确认镜像已推送至 GitHub Container Registry
- 验证 CI 工作流 (`ci.yml`) 能使用构建的镜像运行测试
- 修复任何构建或测试过程中发现的问题
- 确保 workflow_run 触发器正常工作

## Impact
- Affected specs: `ci`
- Affected code:
  - `.github/workflows/docker-build.yml`
  - `.github/workflows/ci.yml`
  - `.github/docker/Dockerfile.p2996`
  - `test/` 目录下的测试文件

## Current Status
- Docker 构建工作流 #21545543637: `in_progress` (已运行约 115 分钟)
- CI 工作流已优化，支持镜像检查和 workflow_run 触发
