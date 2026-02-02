# Change: 完善 CTest 集成

## Why

Boost 标准构建系统要求支持 CMake CTest。当前 CMakeLists.txt 未配置 `enable_testing()` 和 `add_test()`，无法通过 `ctest` 命令运行测试。

## What Changes

- 在根 CMakeLists.txt 添加 `enable_testing()`
- 为所有测试目标添加 `add_test()` 调用
- 支持 `ctest --output-on-failure` 标准测试流程

## Impact

- 修改文件: `CMakeLists.txt`, `test/CMakeLists.txt`
- 关联规范: `specs/build-system`
