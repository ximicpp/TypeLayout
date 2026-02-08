# Change: Analyze Application — Plugin System ABI Compatibility

## Why
插件系统 (如动态加载的 .so/.dll) 需要在宿主程序和插件之间共享数据结构。
ABI 不兼容会导致崩溃、数据损坏等严重问题。TypeLayout 可以在加载时或编译时
验证插件与宿主之间的数据结构兼容性。

## What Changes
- 分析插件系统 ABI 兼容性的典型痛点
- 展示 TypeLayout 在插件接口验证中的工作流
- 对比传统方案 (COM, C ABI,版本号检查)
- 分析 Definition 签名在 ODR 违规检测中的价值
- 识别局限 (虚函数表布局、C++ 名称修饰)

## Impact
- Affected specs: documentation
- Affected code: 无代码变更，纯分析性质
