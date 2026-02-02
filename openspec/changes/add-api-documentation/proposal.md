# Change: 添加详细 API 文档

## Why

Boost 审查要求每个公共函数必须有完整的 API 文档。当前 README 只有 API 概述表格，缺少 Doxygen 风格的详细函数文档。

## What Changes

- 为所有公共 API 添加 Doxygen 风格注释
- 创建 API 参考文档页面
- 包含参数说明、返回值、示例代码

## Impact

- 修改文件: 所有 `include/boost/typelayout/*.hpp` 头文件
- 新增文件: `doc/api/` 参考文档
- 关联规范: `specs/documentation`
