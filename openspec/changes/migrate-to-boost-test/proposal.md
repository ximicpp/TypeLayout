# Change: 迁移到 Boost.Test

## Why

Boost 库的测试需要使用 Boost.Test 框架。当前项目使用自定义测试方式，不符合 Boost 测试标准。

## What Changes

- 将现有测试迁移到 Boost.Test 框架
- 添加更多边界测试用例
- 配置测试自动发现和运行

## Impact

- Affected specs: testing
- Affected code: `test/` 目录
