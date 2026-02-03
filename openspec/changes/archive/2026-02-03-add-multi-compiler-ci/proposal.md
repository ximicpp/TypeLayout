# Change: 添加多编译器 CI

## Why

Boost 库需要支持多个主流编译器。当前 CI 配置仅测试单一编译器，无法保证跨编译器兼容性。

## What Changes

- 配置 GitHub Actions 矩阵构建
- 支持 GCC 11/12/13
- 支持 Clang 14/15/16
- 支持 MSVC 19.3+（当 P2996 可用时）

## Impact

- Affected specs: ci
- Affected code: `.github/workflows/`
