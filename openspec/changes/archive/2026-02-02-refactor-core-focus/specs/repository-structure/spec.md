
## MODIFIED Requirements

### Requirement: 精简仓库结构

TypeLayout 仓库 SHALL 保持极简的纯核心层架构，移除所有非核心文件。

#### Scenario: 核心结构保持完整
- **GIVEN** 精简后的仓库
- **WHEN** 检查目录结构
- **THEN** 应包含：
  - `include/boost/typelayout/core/` 目录 (8个核心文件)
  - `include/boost/typelayout/typelayout.hpp` 入口文件
  - `include/boost/typelayout.hpp` 根入口文件

#### Scenario: 测试目录精简
- **GIVEN** 精简后的仓库
- **WHEN** 检查测试目录
- **THEN** 应包含 4 个核心测试文件：
  - `test_all_types.cpp`
  - `test_signature_comprehensive.cpp`
  - `test_signature_extended.cpp`
  - `test_anonymous_member.cpp`

#### Scenario: 示例目录精简
- **GIVEN** 精简后的仓库
- **WHEN** 检查示例目录
- **THEN** 应包含 4 个核心示例：
  - `demo.cpp` (基础演示)
  - `network_protocol.cpp` (网络协议)
  - `file_format.cpp` (文件格式)
  - `shared_memory_demo.cpp` (共享内存)

#### Scenario: 工具目录精简
- **GIVEN** 精简后的仓库
- **WHEN** 检查工具目录
- **THEN** 应只包含 `typelayout_tool.cpp`

#### Scenario: 所有测试通过
- **GIVEN** 精简后的仓库
- **WHEN** 运行测试套件
- **THEN** 所有 4 个测试应通过
