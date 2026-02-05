# Change: 补充测试用例覆盖

## Why
TypeLayout 有 38 个类型特化，需要确保测试用例覆盖所有情况：
- 验证所有特化都有对应的测试
- 确保新添加的功能（如 `T[]` 错误处理、`requires` 约束）被测试
- 编译运行确保无错误

## What Changes
- 分析现有测试覆盖情况
- 补充缺失的测试用例
- 编译并运行测试直到全部通过

## Impact
- Affected files: `test/*.cpp`
- Scope: 测试覆盖率提升
