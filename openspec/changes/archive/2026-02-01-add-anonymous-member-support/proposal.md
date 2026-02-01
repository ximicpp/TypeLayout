# Change: Add Anonymous Member Support for Layout Signatures

## Why

P2996 静态反射的 `identifier_of()` 对匿名成员（如匿名联合、匿名结构体）会抛出编译错误，
因为匿名成员没有关联的标识符。这导致以下标准库类型无法生成布局签名：

- `std::optional<T>` - 内部使用匿名联合存储值
- `std::variant<Ts...>` - 内部使用匿名联合实现类型擦除
- 用户定义的包含匿名联合/结构体的类型

这个限制在 `audit-layout-signature` 审计中被发现，影响了约 10% 的常用类型。

## What Changes

1. **添加匿名成员检测**: 在调用 `identifier_of()` 前检查成员是否匿名
2. **使用占位名称**: 为匿名成员生成唯一的占位名称（如 `<anon:0>`, `<anon:1>`）
3. **更新签名格式**: 匿名成员在签名中使用 `@offset[<anon:N>]:type` 格式
4. **支持新类型**: 启用对 `std::optional`、`std::variant` 等类型的签名生成

## Impact

- Affected specs: `specs/analysis/spec.md`
- Affected code: 
  - `include/boost/typelayout/detail/reflection_helpers.hpp` (核心变更)
  - `test/test_signature_comprehensive.cpp` (启用跳过的测试)
- 无破坏性变更 - 现有签名格式保持向后兼容
