## MODIFIED Requirements

### Requirement: 示例代码文档
系统 SHALL 提供实用的示例代码，演示常见使用场景。

#### Scenario: 示例代码包含常见场景
- **GIVEN** 示例代码文档
- **WHEN** 浏览示例目录
- **THEN** 应包含以下场景的示例:
  - 基本使用
  - 网络协议验证
  - 共享内存验证
  - 文件格式验证
  - 插件/DLL 接口验证

#### Scenario: 所有示例代码可编译
- **GIVEN** `example/` 目录下的所有 `.cpp` 文件
- **WHEN** 使用 CMake 构建示例目标
- **THEN** 所有示例成功编译

#### Scenario: 插件接口示例演示 ABI 验证
- **GIVEN** 插件接口验证示例
- **WHEN** 用户运行示例
- **THEN** 示例演示插件加载时的布局哈希验证
- **AND** 示例演示布局不匹配时的检测和错误处理
