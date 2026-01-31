## 1. Docker 镜像构建
- [x] 1.1 创建 `.github/docker/` 目录
- [x] 1.2 创建 `Dockerfile.p2996` 镜像定义
- [ ] 1.3 本地测试 Docker 镜像构建 (可选)
- [x] 1.4 创建 `docker-build.yml` 工作流

## 2. GitHub Container Registry 配置
- [x] 2.1 确保仓库 Packages 权限已启用
- [x] 2.2 手动触发 docker-build.yml 构建镜像
- [x] 2.3 验证镜像已推送到 ghcr.io

## 3. CI 工作流配置
- [x] 3.1 创建 `.github/workflows/` 目录
- [x] 3.2 创建 `ci.yml` 主 CI 流水线 (使用 Docker 容器)
- [x] 3.3 创建 `docs.yml` 文档构建检查 (合并到 ci.yml)

## 4. 集成验证
- [ ] 4.1 推送代码触发 CI
- [ ] 4.2 验证 build-and-test job 在容器中运行成功
- [ ] 4.3 验证 docs job 通过
- [x] 4.4 添加状态徽章到 README.md

## 5. 可选增强
- [ ] 5.1 添加代码覆盖率报告
- [ ] 5.2 配置 release 工作流
- [x] 5.3 设置镜像定期重建 (每周) - cron schedule 已配置
