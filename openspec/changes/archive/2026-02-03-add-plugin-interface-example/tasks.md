# Tasks: 添加插件/DLL 接口验证示例

## 1. 创建示例代码

- [x] 1.1 创建 `example/plugin_interface.cpp` 主程序示例
  - 定义插件接口结构体 (PluginInfo, AudioContext, PluginParameter)
  - 实现插件加载和验证逻辑 (PluginHost, verify_plugin_interface)
  - 演示成功加载和布局不匹配两种场景

- [x] 1.2 创建模拟插件代码（同一文件内模拟）
  - 正确版本的插件结构体 (compatible_plugin namespace)
  - 不兼容版本的插件结构体 (incompatible_plugin namespace with V2 structs)

## 2. 更新构建配置

- [x] 2.1 更新 `cmake_root/CMakeLists.txt`
  - 添加 `plugin_interface_example` 构建目标

## 3. Verification
- [x] 3.1 Verify compilation in P2996 Docker container
- [x] 3.2 Verify runtime output matches expected behavior