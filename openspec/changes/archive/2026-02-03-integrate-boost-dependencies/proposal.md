# Change: 整合 Boost 依赖

## Why

考虑是否使用 Boost 基础库（如 Boost.Config）来提高可移植性和与 Boost 生态的一致性。

## What Changes

- 评估是否需要 Boost.Config 进行编译器特性检测
- 评估是否需要其他 Boost 基础库
- 保持依赖最小化原则

## Impact

- Affected specs: dependencies
- Affected code: 头文件 include 语句

## Note

此项为可选改进，优先级较低。当前项目无外部依赖是一个优势。
