## Context

本文档是 Layout 签名（第一层）的系统性正确性和完备性审计报告。
审计基于 `signature_detail.hpp`、`signature.hpp`、`fwd.hpp` 源码、`signature` spec 以及 `test_two_layer.cpp` 测试用例。

Layout 签名的核心保证是 **V1 可靠性**：
> `layout_sig(T) == layout_sig(U)` implies `memcmp-compatible(T, U)`

## 审计方法

对 Layout 签名的每个编码维度逐项审查：
1. **格式正确性**：生成的签名格式是否与 spec 一致
2. **展平正确性**：继承和组合是否正确展平为绝对偏移
3. **区分能力**：布局不同的类型是否产生不同签名
4. **归一能力**：布局相同的类型是否产生相同签名
5. **V1 保证**：签名相同是否真正意味着 memcmp 兼容

---

## 1. 架构前缀 — ✅ 正确

**代码位置**: `signature.hpp:12-19`

**实现**:
```cpp
consteval auto get_arch_prefix() noexcept {
    if constexpr (sizeof(void*) == 8)
        return FixedString{TYPELAYOUT_LITTLE_ENDIAN ? "[64-le]" : "[64-be]"};
    else if constexpr (sizeof(void*) == 4)
        return FixedString{TYPELAYOUT_LITTLE_ENDIAN ? "[32-le]" : "[32-be]"};
    else
        static_assert(...);
}
```

**审计**:
- 指针大小 (32/64) 正确区分 — ✅
- 字节序 (le/be) 正确区分 — ✅
- 不支持的指针大小触发 `static_assert` — ✅
- 前缀格式 `[64-le]` 与 spec 一致 — ✅

**V1 影响**: 架构前缀确保不同平台的签名不会误匹配。这是正确的，因为不同架构上的类型布局通常不兼容。

---

## 2. record 类型（struct/class）Layout — ✅ 正确

**格式**: `record[s:SIZE,a:ALIGN]{CONTENT}` 或 `record[s:SIZE,a:ALIGN,vptr]{CONTENT}`

**审计**:
- size 编码自 `sizeof(T)` — ✅
- alignment 编码自 `alignof(T)` — ✅
- `vptr` 标记通过 `std::is_polymorphic_v<T>` 检测 — ✅
- 内容由 `get_layout_content<T>()` 生成 — ✅

---

## 3. 继承展平 — ✅ 正确

**代码位置**: `signature_detail.hpp:222-234`

**机制**: `layout_one_base_prefixed` 递归到基类，将基类偏移 (`offset_of(base_info).bytes`) 加入 `OffsetAdj`。
基类字段的最终偏移 = `offset_of(member).bytes + OffsetAdj(accumulated)`，这就是绝对偏移。

**审计**:
- 单继承展平 (`Derived : Base`) — ✅ 测试通过 (`Derived == Flat`)
- 多级继承展平 (`C : B : A`) — ✅ 测试通过 (`C == Flat`)
- 多重继承展平 (`Multi : Base1, Base2`) — ✅ 测试通过 (`Multi == Flat`)
- 空基类优化 (`WithEmpty : Empty`) — ✅ 测试通过 (`WithEmpty == Plain`)
- 虚继承展平 — ✅ 测试验证两个 i32 字段都出现

**V1 影响**: 展平为绝对偏移确保签名只反映内存中的实际字段位置，与继承结构无关。✅ 正确。

---

## 4. 组合展平 — ✅ 正确

**代码位置**: `signature_detail.hpp:205-207`

**机制**: 当字段类型是 class（非 union）时，不编码为 `record{...}`，而是递归调用 `layout_all_prefixed<FieldType, field_offset>()`，将嵌套字段直接展开到外层的绝对偏移位置。

```cpp
} else if constexpr (std::is_class_v<FieldType> && !std::is_union_v<FieldType>) {
    constexpr std::size_t field_offset = offset_of(member).bytes + OffsetAdj;
    return layout_all_prefixed<FieldType, field_offset>();
}
```

**审计**:
- 单层组合 (`Composed{Inner x}` vs `Flat{int a; int b}`) — ✅ 匹配
- 深层组合 (`Outer{Mid{Deep{..}}}` vs `DeepFlat`) — ✅ 匹配
- Union 成员不展平（union 内的 struct 保留为 `record{...}`）— ✅ 正确

**V1 影响**: 组合展平正确体现了 "内存中的实际字节排列"。两个类型只要叶字段在相同绝对偏移、相同类型，就是 memcmp 兼容的。✅

---

## 5. Union 处理 — ✅ 正确

**格式**: `union[s:SIZE,a:ALIGN]{MEMBERS}`

**代码位置**: `signature_detail.hpp:252-299`

**审计**:
- Union 成员使用 `layout_union_field`（不展平 struct 成员）— ✅
- 每个成员编码为 `@OFFSET:TYPE_SIG`（无字段名）— ✅
- 两个相同 union 签名匹配 — ✅ 测试通过

**V1 影响**: Union 成员不展平是正确的，因为 union 的字段共享内存。展平会丢失 "共享同一地址" 的语义信息。保留原子 `record{...}` 让签名精确反映内存结构。✅

---

## 6. 位域 — ✅ 正确

**格式**: `@BYTE.BIT:bits<WIDTH,TYPE_SIG>`

**代码位置**: `signature_detail.hpp:194-204`

**审计**:
- 字节偏移 + 位偏移使用 `offset_of(member)` — ✅
- 位域偏移**加上 OffsetAdj** 正确累计绝对偏移 — ✅
- 位宽使用 `bit_size_of(member)` — ✅
- 底层类型递归 — ✅
- 无字段名（Layout 模式）— ✅

**⚠️ 潜在问题**: 位域的 `bit_off.bytes + OffsetAdj` 计算了字节偏移的绝对值，但 `bit_off.bits` 仍是相对位偏移。这是正确的，因为位偏移始终是相对于当前字节的 (0-7)。✅

---

## 7. TypeSignature 特化审计

### 7.1 固定宽度整数 — ✅ 正确

| 类型 | 签名 | 验证 |
|------|------|------|
| `int8_t` | `i8[s:1,a:1]` | ✅ |
| `uint8_t` | `u8[s:1,a:1]` | ✅ |
| `int16_t` | `i16[s:2,a:2]` | ✅ |
| `uint16_t` | `u16[s:2,a:2]` | ✅ |
| `int32_t` | `i32[s:4,a:4]` | ✅ |
| `uint32_t` | `u32[s:4,a:4]` | ✅ |
| `int64_t` | `i64[s:8,a:8]` | ✅ |
| `uint64_t` | `u64[s:8,a:8]` | ✅ |

### 7.2 类型别名去重 — ✅ 正确

**问题场景**: 在某些平台上 `int32_t == int` 或 `int64_t == long`。如果同时定义了两个特化，会导致重复特化错误。

**解决方案**: 使用 `requires` 约束条件防止重复：
```cpp
template <SignatureMode Mode>
    requires (!std::is_same_v<long, int32_t> && !std::is_same_v<long, int64_t>)
struct TypeSignature<long, Mode> { ... };
```

**审计**:
- `signed char` / `unsigned char` 与 `int8_t` / `uint8_t` 的去重 — ✅
- `long` 与 `int32_t` / `int64_t` 的去重 — ✅
- `unsigned long` 与 `uint32_t` / `uint64_t` 的去重 — ✅
- `long long` 与 `int64_t` 的去重 — ✅
- `unsigned long long` 与 `uint64_t` 的去重 — ✅

**V1 影响**: 确保在同一平台上，`long` 和 `int32_t`（如果相同）产生完全相同的签名。✅

### 7.3 平台相关类型 — ✅ 正确

| 类型 | 策略 | 验证 |
|------|------|------|
| `long` | 根据 `sizeof(long)` 选择 `i32`/`i64` | ✅ |
| `long double` | 使用 `format_size_align("f80", sizeof, alignof)` | ✅ |
| `wchar_t` | 使用 `format_size_align("wchar", sizeof, alignof)` | ✅ |

### 7.4 浮点类型 — ✅ 正确

| 类型 | 签名 | 验证 |
|------|------|------|
| `float` | `f32[s:4,a:4]` | ✅ |
| `double` | `f64[s:8,a:8]` | ✅ |
| `long double` | `f80[s:SIZE,a:ALIGN]`（平台相关）| ✅ |

### 7.5 字符类型 — ✅ 正确

| 类型 | 签名 | 验证 |
|------|------|------|
| `char` | `char[s:1,a:1]` | ✅ |
| `char8_t` | `char8[s:1,a:1]` | ✅ |
| `char16_t` | `char16[s:2,a:2]` | ✅ |
| `char32_t` | `char32[s:4,a:4]` | ✅ |
| `wchar_t` | `wchar[s:SIZE,a:ALIGN]` | ✅ |

**⚠️ 观察**: `char` 和 `int8_t`/`uint8_t` 产生不同签名（`char[s:1,a:1]` vs `i8[s:1,a:1]` / `u8[s:1,a:1]`）。这是正确的，因为 `char` 的有符号性是实现定义的，它是独立的类型。但需要注意：**两个分别使用 `char` 和 `int8_t` 字段的 struct 不会 Layout 匹配**。这是保守但正确的行为 — 签名反映的是编译器眼中的类型，而非仅仅字节大小。✅

### 7.6 其他基本类型 — ✅ 正确

| 类型 | 签名 | 验证 |
|------|------|------|
| `bool` | `bool[s:1,a:1]` | ✅ |
| `std::nullptr_t` | `nullptr[s:SIZE,a:ALIGN]` | ✅ |
| `std::byte` | `byte[s:1,a:1]` | ✅ |

### 7.7 指针和引用 — ✅ 正确

| 类型 | 签名 | 验证 |
|------|------|------|
| `T*` | `ptr[s:SIZE,a:ALIGN]` | ✅ |
| `T&` | `ref[s:SIZE,a:ALIGN]` | ✅ |
| `T&&` | `rref[s:SIZE,a:ALIGN]` | ✅ |
| `T C::*` | `memptr[s:SIZE,a:ALIGN]` | ✅ |

**⚠️ 观察**: 指针类型**不编码被指类型**。`int*` 和 `double*` 产生相同签名 `ptr[s:8,a:8]`。这是正确的 — 从布局角度看，所有指针占用相同的内存空间。✅

### 7.8 函数指针 — ✅ 正确

所有变体（普通/noexcept/variadic）归一化为 `fnptr[s:SIZE,a:ALIGN]`。✅

**⚠️ 缺少特化**: `R(*)(Args..., ...) noexcept`（variadic + noexcept 组合）没有特化。这不会报错（会落入 `T*` 特化，生成 `ptr[s:8,a:8]`），但语义标签不精确。实际影响很小（布局相同），严格来说不是 V1 违反。

### 7.9 CV 限定 — ✅ 正确

`const T`、`volatile T`、`const volatile T` 都剥除并转发到 `TypeSignature<T, Mode>`。
这是正确的 — CV 限定不影响内存布局。✅

### 7.10 数组 — ✅ 正确

**格式**: `array[s:SIZE,a:ALIGN]<ELEMENT_SIG,N>` 或 `bytes[s:N,a:1]`

**审计**:
- 字节数组归一化（`char[]`, `uint8_t[]`, `std::byte[]`, `signed char[]`, `unsigned char[]`, `int8_t[]`, `char8_t[]`）— ✅ 全部归一到 `bytes[s:N,a:1]`
- 非字节数组保留元素类型和计数 — ✅
- 无界数组 `T[]` 触发 `static_assert` — ✅

**V1 影响**: 字节数组归一化正确 — 不同字节类型的数组在内存中完全相同。✅

### 7.11 枚举类型 — ✅ 正确

**Layout 格式**: `enum[s:SIZE,a:ALIGN]<UNDERLYING_SIG>`

**审计**:
- Layout 模式不包含限定名（只有底层类型）— ✅
- 不同枚举但底层类型相同 → Layout 匹配 — ✅ 测试通过
- 底层类型递归 — ✅

---

## 8. V1 可靠性保证验证

**命题**: `layout_sig(T) == layout_sig(U) ⟹ memcmp-compatible(T, U)`

### 8.1 签名编码了什么

Layout 签名包含以下信息：
1. **架构前缀** — 平台指针大小 + 字节序
2. **外层容器** — `record`/`union`/`enum`/`array`/基本类型
3. **size 和 alignment** — 精确到字节
4. **所有叶字段的绝对偏移** — 继承和组合均已展平
5. **每个叶字段的类型签名** — 递归到基本类型
6. **多态标记** — `vptr`

### 8.2 可能破坏 V1 的场景分析

| 场景 | 分析 | V1 成立? |
|------|------|----------|
| 相同字段不同顺序 | 偏移量不同 → 签名不同 | ✅ 不会误匹配 |
| 尾部填充不同 | `sizeof(T)` 不同 → 签名不同 | ✅ 不会误匹配 |
| 中间填充不同 | 下一个字段的偏移不同 → 签名不同 | ✅ 不会误匹配 |
| `[[no_unique_address]]` | 编译器可能重用填充空间，改变偏移 | ✅ `offset_of` 反映实际偏移 |
| 虚继承 | vptr 和 vbase 偏移由 `offset_of` 捕获 | ✅ |
| 位域填充 | `offset_of` 提供精确的字节.位偏移 | ✅ |

### 8.3 ⚠️ 唯一边界情况：尾部填充的 memcpy 语义

```cpp
struct A { int32_t x; char y; };   // sizeof = 8 (3 bytes padding)
struct B { int32_t x; char y; };   // sizeof = 8 (3 bytes padding)
```

A 和 B 的 Layout 签名相同。`memcmp(a, b, sizeof(A))` 会比较填充字节。
如果填充字节未初始化，`memcmp` 结果不确定。

**这不是签名系统的缺陷**，而是 `memcmp` 本身的局限。签名正确反映了 "两个类型在内存中有相同的字段排列"。用户在使用 `memcmp` 前应当零初始化或使用 `memset`。

**结论**: V1 保证在 "零初始化前提下" 严格成立。✅

---

## 9. 测试覆盖完备性评估

| Layout 编码维度 | 有测试? | 测试类型 |
|-----------------|---------|----------|
| 架构前缀 `[64-le]` | ✅ | 精确匹配（Simple struct） |
| record 基本格式 | ✅ | 精确匹配 |
| size + alignment | ✅ | 精确匹配 + alignas 测试 |
| 单继承展平 | ✅ | 等价测试 (`Derived == Flat`) |
| 多级继承展平 | ✅ | 等价测试 (`C == Flat`) |
| 多重继承展平 | ✅ | 等价测试 (`Multi == Flat`) |
| 空基类优化 | ✅ | 等价测试 (`WithEmpty == Plain`) |
| 虚继承展平 | ✅ | 字段存在性验证 |
| 组合展平（单层）| ✅ | 等价测试 (`Composed == Flat`) |
| 组合展平（深层）| ✅ | 等价测试 (`Outer == DeepFlat`) |
| 多态 `vptr` | ✅ | 存在性 + 不匹配测试 |
| 枚举 Layout | ✅ | 精确匹配 + 跨枚举匹配 |
| 位域 `bits<>` | ✅ | 存在性检查 |
| Union 不展平 | ✅ | 等价 + `record` 存在性 |
| 字节数组归一化 | ✅ | 三种字节类型互匹配 |
| alignas 影响 | ✅ | 匹配 + 不匹配测试 |
| 基本类型 (int, float...) | ✅ | 精确匹配（嵌套在 struct 内）|
| 指针/引用类型 | ❌ | 无独立测试 |
| 函数指针类型 | ❌ | 无独立测试 |
| `long double` / `wchar_t` 平台类型 | ❌ | 无独立测试 |
| 匿名成员 Layout | ❌ | 仅有 Definition 测试 |
| 多维数组 `T[M][N]` | ❌ | 无测试 |
| 带数组字段的 struct | ❌ | 无测试 |
| `const`/`volatile` 字段 | ❌ | 无测试 |

---

## 10. 发现的问题

### 🟢 P3: 缺少 Layout 层的独立类型测试

**观察**: 以下类型在 Layout 模式下缺少独立测试：
- 指针类型 (`int*`, `void*` 等)
- 引用类型 (`int&`, `int&&`)
- 函数指针类型
- `long double`、`wchar_t`
- 匿名成员在 Layout 模式下的行为
- 多维数组 `int[3][4]`
- 带数组字段的 struct
- CV 限定字段 (`const int x`)

**影响**: 这些类型的 TypeSignature 特化在代码层面看起来是正确的，但缺少测试验证。
如果测试在 Docker 中确实能通过，建议补充。

**严重程度**: 🟢 低 — 代码逻辑清晰，不太可能有隐藏缺陷

### 🟡 P2: 函数指针 variadic+noexcept 组合缺少特化

**代码位置**: `signature_detail.hpp:386-406`

**观察**: 存在三个函数指针特化：
1. `R(*)(Args...)` — 普通
2. `R(*)(Args...) noexcept` — noexcept
3. `R(*)(Args..., ...)` — variadic

**缺少**: `R(*)(Args..., ...) noexcept`（variadic + noexcept）

**影响**: 这种类型会落入通用 `T*` 特化，产生 `ptr[s:8,a:8]` 而非 `fnptr[s:8,a:8]`。
布局相同（指针都是 8 字节），所以 **V1 保证不受影响**。但语义标签不精确。

---

## 11. 总结

Layout 签名系统整体设计**正确且符合 spec**，V1 可靠性保证在所有已分析场景下成立。

| 维度 | 结论 |
|------|------|
| 继承展平 | ✅ 完全正确 |
| 组合展平 | ✅ 完全正确 |
| 绝对偏移计算 | ✅ 完全正确 |
| 类型特化覆盖 | ✅ 基本完备 |
| Union 处理 | ✅ 完全正确 |
| 位域处理 | ✅ 完全正确 |
| 多态标记 | ✅ 完全正确 |
| 平台兼容性 | ✅ 正确处理类型别名冲突 |
| V1 可靠性保证 | ✅ 成立 |

**发现 0 个正确性缺陷**。
**发现 1 个 P2 语义标签缺失**（variadic+noexcept 函数指针）。
**发现 1 个 P3 测试覆盖缺口**（7 个类型维度缺少独立测试）。
