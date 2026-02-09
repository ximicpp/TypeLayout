# Change: 插件 ABI / ODR 检测场景与两层签名的关系分析

## Why
动态加载插件（dlopen/LoadLibrary）时，宿主程序与插件可能独立编译，导致同名结构体的布局不一致（ABI 不兼容）甚至定义不同（ODR 违规）。这是两层签名同时发挥作用的典型场景：Layout 层验证二进制兼容性，Definition 层检测 ODR 违规。需要分析这两层在插件架构中的精确角色和形式化保证。

## What Changes
- 新增插件 ABI/ODR 检测场景的深度分析文档
- 分析内容包括：
  - Layout 层在 dlopen 加载时的 ABI 验证角色
  - Definition 层检测 ODR 违规的独特能力
  - 编译时 vs 运行时验证的两种模式
  - 形式化保证链：为什么 Definition 不匹配 ≠ ABI 不兼容，但暗示 ODR 违规

## Impact
- Affected specs: signature
- Affected code: 无代码变更
