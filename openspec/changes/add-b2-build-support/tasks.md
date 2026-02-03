# Tasks: 添加 B2 构建支持

## 1. 核心配置

- [x] 1.1 创建根目录 `Jamfile`
  - 定义 header-only 库
  - 配置 P2996 编译器标志
  - 设置 include 路径
- [x] 1.2 创建 `example/Jamfile` 示例构建配置
- [x] 1.3 创建 `test/Jamfile` 测试构建配置
- [x] 1.4 更新 `meta/libraries.json` 库元数据

## 2. 目标配置

- [x] 2.1 配置 header-only 库目标 (`alias boost_typelayout`)
- [x] 2.2 配置示例构建目标 (5 个示例)
- [x] 2.3 配置测试构建目标 (编译测试 + 运行测试)

## 3. 验证

- [ ] 3.1 使用 B2 成功构建示例
- [ ] 3.2 使用 B2 成功运行测试
- [ ] 3.3 确认与 CMake 构建结果一致

## 备注

**重要限制**: B2 构建需要安装有 P2996 支持的 Bloomberg Clang。
当前只能在 P2996 Docker 容器中验证。本地 Windows 环境无法测试。

验证步骤（在 Docker 容器中）:
```bash
cd /path/to/TypeLayout
b2 toolset=clang
b2 toolset=clang test
```