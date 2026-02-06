## Why

经过深度代码审查，发现上一轮修复（fix-code-quality-issues）后仍存在若干代码质量问题。
这些问题虽非阻塞性，但影响代码的健壮性、可维护性和用户体验。

## What Changes

1. **CompileString(string_view) 构造函数未初始化问题**
   - 添加 `: value{}` 成员初始化，消除潜在未定义行为

2. **LayoutSupported concept 硬错误问题**
   - 添加 `has_determinable_layout_v<T>` 类型特征
   - concept 使用特征检查，避免对 `void` 等类型触发 static_assert

3. **NumberBufferSize 命名不一致**
   - 统一定义到 `config.hpp`
   - 两处引用统一常量名

4. **废弃函数缺少 [[deprecated]] 属性**
   - 为 `get_layout_hash_from_signature` 添加 `[[deprecated]]` 属性
   - 简化实现为直接委托

5. **prefix lambda 冗余分支**
   - 合并三个相同返回值的分支为一个条件

6. **void* 冗余特化**
   - 删除与 `T*` 特化生成相同结果的 `void*` 特化

## Impact

- Affected specs: signature
- Affected files:
  - `include/boost/typelayout/core/compile_string.hpp`
  - `include/boost/typelayout/core/config.hpp`
  - `include/boost/typelayout/core/concepts.hpp`
  - `include/boost/typelayout/core/signature.hpp`
  - `include/boost/typelayout/core/type_signature.hpp`
  - `include/boost/typelayout/core/reflection_helpers.hpp`
