## ADDED Requirements

### Requirement: Container Runtime Library Path
CI 容器环境 SHALL 配置正确的运行时库搜索路径，确保 P2996 工具链编译的二进制能够找到 `libc++.so.1`。

#### Scenario: CTest 运行测试成功
- **WHEN** CTest 在容器内运行测试二进制
- **THEN** 测试进程能找到 `/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu/libc++.so.1`
- **AND** 所有测试通过

#### Scenario: 环境变量继承
- **WHEN** CI job 使用 `container:` 指令运行
- **THEN** `LD_LIBRARY_PATH` 环境变量被设置为包含 P2996 工具链库路径
- **AND** 所有 step 都能继承此环境变量