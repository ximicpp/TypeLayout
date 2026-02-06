# Change: Add Physical Layout Layer (Two-Layer Architecture)

## Why

当前 TypeLayout 的 `Structural` 签名混合了两种本质不同的信息：

1. **物理事实** — 字节在内存中的排列（偏移、大小、对齐）
2. **结构语义** — C++ 对象模型赋予的含义（继承层次、多态标记、子对象边界）

混合编码的问题：
- **False Negative**: 物理布局相同但结构不同的类型被判定为不匹配（如 `Derived : Base` vs `Flat`）
- **场景限制**: 用户无法选择只验证物理布局（对 C 互操作、序列化、共享内存场景很重要）
- **保证边界不清晰**: Structural 签名承诺的是什么？

核心目标：`Physical` 模式下，布局相同的 `Derived : Base` 与 `Flat` 产生**完全相同**的签名。

## What Changes

### 新增 Physical 签名模式

```cpp
enum class SignatureMode {
    Physical,    // 新增：纯字节布局，继承扁平化，无对象模型标记
    Structural,  // 现有：布局 + C++ 对象模型信息（默认）
    Annotated    // 现有：完整信息含成员名
};
```

三者关系层级：`Physical` < `Structural` < `Annotated`

### Physical vs Structural 对比

| 特性 | Physical | Structural |
|------|----------|------------|
| 类型前缀 | `record` (统一) | `struct` / `class` |
| 大小/对齐 | ✅ | ✅ |
| 字段偏移 | ✅ | ✅ |
| 字段类型（递归） | ✅ | ✅ |
| 继承标记 (`inherited`) | ❌ | ✅ |
| 多态标记 (`polymorphic`) | ❌ | ✅ |
| 基类前缀 (`~base:`) | ❌ 扁平化 | ✅ |
| 虚基类前缀 (`~vbase:`) | ❌ 跳过 | ✅ |
| 字节数组归一化 | ✅ `bytes[]` | ✅ `bytes[]` |

### 关键设计决策

**继承扁平化**：
```cpp
struct Base { int x; };
struct Derived : Base { double y; };
struct Flat { int x; double y; };

// Physical 层：签名相同（布局完全一致）
// Structural 层：签名不同（结构语义不同）
```

**vtable 指针处理**：
- Physical 层：不显式编码 vtable 指针，通过字段偏移隐式体现
- Structural 层：标记为 `polymorphic`

**虚基类处理 (v1 限制)**：
- Physical 层递归展开时跳过虚基类（偏移依赖最终派生类型，无法可靠计算）
- 类型的 size/alignment 仍正确反映虚基类存在

### API 变更

```cpp
// 新增便捷函数
template<typename T>
consteval auto get_physical_signature();

template<typename T1, typename T2>
consteval bool physical_signatures_match();

template<typename T>
consteval uint64_t get_physical_hash();

// 新增 concepts
template<typename T, typename U>
concept PhysicalLayoutCompatible;

// 新增宏
TYPELAYOUT_ASSERT_PHYSICAL_COMPATIBLE(Type1, Type2)
TYPELAYOUT_BIND_PHYSICAL(Type, ExpectedSig)
```

## Impact

- **Affected specs**: `signature`
- **Affected code**: 
  - `config.hpp` — 枚举扩展
  - `compile_string.hpp` — 新增 `skip_first()` 
  - `reflection_helpers.hpp` — 新增扁平化模块 (~120 行)
  - `type_signature.hpp` — Physical 分支处理
  - `signature.hpp` — Physical API
  - `concepts.hpp` — Physical concepts
  - `verification.hpp` — Physical 验证
- **BREAKING**: None — 新增模式，现有行为完全不变
- **Migration**: 用户可选择使用 Physical 层以获得更精确的数据兼容性保证
- **估计改动量**: ~230 行新增，0 行删除

## Known Limitations (v1)

1. **虚基类不参与扁平化** — 虚基类字段不出现在 Physical 扁平字段列表中
2. **Physical 签名匹配 ≠ memcpy 安全** — 不保证 trivially copyable
3. **`record` vs `struct`/`class`** — Physical 与 Structural 签名永远不等（设计意图）
