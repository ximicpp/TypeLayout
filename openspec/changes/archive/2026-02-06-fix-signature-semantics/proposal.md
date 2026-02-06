# Change: 修复签名语义 - 解耦成员名称与布局身份

## Why

当前实现违反了核心保证 "签名相同 ⟺ 布局相同"。

**问题**：签名中嵌入了成员名称，导致布局相同但名称不同的类型产生不同签名：

```cpp
struct A { uint32_t x; uint64_t y; };
struct B { uint32_t a; uint64_t b; };

// 布局完全相同（sizeof、alignof、所有偏移量一致）
// 但签名不同，因为包含了 "x","y" vs "a","b"
static_assert(get_layout_signature<A>() != get_layout_signature<B>()); // BUG!
```

这是一个**形式逻辑缺陷**，会导致 Boost 审查被拒。

## What Changes

1. **引入双模式签名**：
   - `SignatureMode::Structural` (默认) - 仅包含布局信息，无名称
   - `SignatureMode::Annotated` - 包含名称，用于调试诊断

2. **修改 API**：
   - `get_layout_signature<T>()` 默认返回结构化签名（无名称）
   - 新增 `get_structural_signature<T>()` 和 `get_annotated_signature<T>()` 便捷别名
   - `get_layout_hash<T>()` 始终基于结构化签名计算

3. **修改签名格式**：
   - Structural: `"[64-le]struct[s:16,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8]}"`
   - Annotated: `"[64-le]struct[s:16,a:8]{@0[x]:u32[s:4,a:4],@8[y]:u64[s:8,a:8]}"`

4. **更新概念定义**：
   - 所有比较概念基于 `SignatureMode::Structural`

5. **更新文档**：
   - 修正核心保证表述为 "结构化签名相同 ⟺ 布局相同"

## Impact

- **BREAKING**: 签名格式变更，默认不再包含名称
- Affected specs: `signature`
- Affected code: 
  - `include/boost/typelayout/core/signature.hpp`
  - `include/boost/typelayout/core/hash.hpp`
  - `include/boost/typelayout/core/concepts.hpp`
  - `README.md`

## Rationale

核心保证的数学正确性：

$$\sigma_{\text{structural}}(T_1) = \sigma_{\text{structural}}(T_2) \iff \mathcal{L}(T_1) = \mathcal{L}(T_2)$$

成员名称是**语义信息**，不是**布局信息**。将它们分离到 Annotated 模式：
- 保证形式正确性
- 保留调试便利性
- 为未来跨语言兼容预留空间
