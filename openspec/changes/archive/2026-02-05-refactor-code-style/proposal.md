# Change: 代码风格重构 - 去AI化与极简注释

## Why

当前代码存在典型的"AI生成"痕迹：
- 过多冗余注释（如 "// Integers", "// Floating point" 等分类注释）
- 大量分隔线 `//===...===` 和 `// =========...========` 
- 注释解释显而易见的内容（如 "// const/volatile should be stripped"）
- 过度使用"Note:"、"Suggestion:"等说教式语气
- 测试文件中的节号标注（如 "// 1. Primitive Types"）

好代码应该是自解释的，注释只在必要时出现。

## What Changes

### 库代码 (include/boost/typelayout/)
- 删除冗余分类分隔线
- 精简重复注释，代码自解释
- 保留必要的平台差异说明和非显而易见的逻辑
- 保留 Boost 版权头和 include guards

### 测试代码 (test/)
- 移除节号标注（"//=== 1. xxx ==="）
- 空行分组替代注释分隔
- 删除解释测试目的的显而易见注释
- 保留真正有价值的测试说明（如平台差异、边界情况）

## Impact
- Affected specs: 无
- Affected code: 
  - `include/boost/typelayout/core/type_signature.hpp`
  - `include/boost/typelayout/core/signature.hpp`
  - `include/boost/typelayout/core/verification.hpp`
  - `test/test_all_types.cpp`
  - `test/test_signature_extended.cpp`
