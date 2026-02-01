## 1. 实现核心库组件

- [x] 1.1 创建 `include/boost/typelayout/compat.hpp`
  - [x] 1.1.1 定义 `Platform` 枚举（linux_x64, windows_x64, etc.）
  - [x] 1.1.2 实现 `TYPELAYOUT_TYPES(...)` 宏
  - [x] 1.1.3 实现 `TYPELAYOUT_PLATFORMS(...)` 宏
  - [x] 1.1.4 实现 `emit_signatures<TypeList>()` 函数模板

- [x] 1.2 创建 `tools/siggen.cpp`
  - [x] 1.2.1 通过 `-D` 宏 include 用户配置文件
  - [x] 1.2.2 调用 `emit_signatures<>()` 输出签名
  - [x] 1.2.3 输出格式: `__PLATFORM__ xxx` + `TypeName Hash Size Align`

- [x] 1.3 创建 `tools/compare_signatures.cpp`
  - [x] 1.3.1 解析签名文件格式
  - [x] 1.3.2 构建类型->平台->签名映射
  - [x] 1.3.3 比对所有类型的 hash
  - [x] 1.3.4 输出兼容/不兼容报告
  - [x] 1.3.5 不兼容时返回非零退出码

## 2. 实现 CI 组件

- [x] 2.1 创建 `.github/workflows/compat-check.yml`
  - [x] 2.1.1 定义 `workflow_call` 触发器
  - [x] 2.1.2 定义 inputs: config, platforms
  - [x] 2.1.3 实现 setup job（解析平台矩阵）
  - [x] 2.1.4 实现 generate job（多平台并行生成签名）
  - [x] 2.1.5 实现 compare job（汇总比对）

- [x] 2.2 创建平台映射配置
  - [x] 2.2.1 linux-x64 -> ubuntu-latest
  - [x] 2.2.2 windows-x64 -> windows-latest
  - [x] 2.2.3 macos-arm64 -> macos-latest
  - [x] 2.2.4 linux-arm64 -> ubuntu-latest (QEMU)

## 3. 测试验证

- [x] 3.1 创建测试用例 `example/typelayout.config.example.hpp`
  - [x] 3.1.1 定义包含 portable 类型的配置 (NetworkPacket, PlayerState, SensorReading)
  - [x] 3.1.2 定义包含 non-portable 类型的配置 (BadLongType, BadWcharType)
  - [x] 3.1.3 验证签名生成器输出格式
  - [x] 3.1.4 验证比对工具检测结果

- [x] 3.2 本地 WSL 测试
  - [x] 3.2.1 编译签名生成器 ✅
  - [x] 3.2.2 生成测试签名 ✅
  - [x] 3.2.3 运行比对工具 ✅ (兼容/不兼容场景均通过)

## 4. 文档更新

- [x] 4.1 更新 README.md
  - [x] 4.1.1 添加 "CI Compatibility Check" 章节
  - [x] 4.1.2 添加快速开始示例
  - [x] 4.1.3 添加支持平台列表

- [ ] 4.2 创建 `docs/compat-tool.md` 详细文档 (可选，后续完善)
  - [ ] 4.2.1 工具架构说明
  - [ ] 4.2.2 配置选项参考
  - [ ] 4.2.3 故障排除指南

## 5. Spec 更新

- [x] 5.1 创建 `specs/compat-tool/spec.md` (在 delta 中)
- [ ] 5.2 更新 `specs/ci/spec.md` 添加 reusable workflow 规格 (可选)
