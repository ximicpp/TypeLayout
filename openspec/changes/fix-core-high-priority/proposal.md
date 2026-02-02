# Change: 修复 TypeLayout.Core 高优先级问题

## Why

根据 `core-completeness.adoc` 分析报告，TypeLayout.Core 在 Boost 提交前需要解决以下高优先级问题：

1. 缺少 `std::array<T, N>` 显式特化
2. 编译错误诊断信息不够详细
3. 缺少零宽度位域测试
4. 虚拟继承限制未文档化

这些问题会影响库的完备性和用户体验，需要在 Boost 正式审查前解决。

## What Changes

### 1. 类型支持扩展
- **ADDED**: `std::array<T, N>` 显式特化
- **ADDED**: `std::pair<T1, T2>` 显式特化
- **ADDED**: `std::span<T, Extent>` 显式特化 (C++20)

### 2. 错误诊断增强
- **MODIFIED**: 改进不支持类型的 `static_assert` 错误信息
- **ADDED**: 类型诊断宏 `TYPELAYOUT_STATIC_ASSERT_TYPE`

### 3. 测试补充
- **ADDED**: 零宽度位域测试用例
- **ADDED**: 超深继承层次测试 (10+ 层)
- **ADDED**: `std::array` 签名测试

### 4. 文档更新
- **MODIFIED**: 添加虚拟继承限制说明到用户指南

## Impact

- Affected specs: `core-signatures`, `util-concepts`
- Affected code: 
  - `include/boost/typelayout/core/type_signature.hpp`
  - `include/boost/typelayout/core/config.hpp`
  - `test/test_all_types.cpp`
  - `test/test_edge_cases.cpp` (新建)
  - `doc/modules/ROOT/pages/user-guide/inheritance.adoc`
