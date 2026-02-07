# 两层签名正确性与完备性审计

## 方法论
对 `openspec/specs/signature/spec.md` 中每条 Requirement / Scenario 逐一审计：
1. **代码是否实现** — 找到对应的代码行
2. **实现是否正确** — 推演边界情况
3. **测试是否覆盖** — 找到对应的 static_assert
4. **spec 是否完备** — 实现中有但 spec 没写的行为

---

## A. 逐条审计

### A1. Requirement: Two-Layer Signature Architecture

#### Scenario: Layout signature (Layer 1)
| 条款 | 代码位置 | 正确性 | 测试覆盖 |
|------|----------|--------|----------|
| size, alignment, arch prefix | `type_signature.hpp:217-231` `signature.hpp:27` | ✅ | ✅ `test_basic::Simple` |
| 继承展平 | `reflection_helpers.hpp:237-245` | ✅ | ✅ `test_inheritance` |
| 组合展平 | `reflection_helpers.hpp:219-222` | ✅ | ✅ `test_composition_flatten` |
| 无字段名 | `reflection_helpers.hpp:224-228` — 不调用 `get_member_name` | ✅ | ✅ `Simple` vs `Simple2` |
| vptr 标记 | `type_signature.hpp:214-222` | ✅ | ✅ `test_polymorphic` |
| **缺失**: union 成员不展平 | `type_signature.hpp:206-209` 用 `get_layout_union_content` | ✅ | ✅ `test_union` |

**Spec 遗漏**: spec 第 17-23 行未提及 "union 成员不展平"。代码已实现但 spec 没有记录。

#### Scenario: Definition signature (Layer 2)
| 条款 | 代码位置 | 正确性 | 测试覆盖 |
|------|----------|--------|----------|
| size, align, arch, field names | `type_signature.hpp:233-244` | ✅ | ✅ |
| `~base<QualifiedName>` | `reflection_helpers.hpp:155-156` | ✅ | ✅ |
| `~vbase<QualifiedName>` | `reflection_helpers.hpp:151-153` | ✅ | ✅ (虚继承测试) |
| `,polymorphic` 标记 | `type_signature.hpp:236-237` | ✅ | ✅ |
| 限定名 | `reflection_helpers.hpp:27-47` | ✅ | ✅ |
| 枚举限定名 | `type_signature.hpp:184-191` | ✅ | ✅ |

**正确性问题 D1**: Definition 模式下嵌套 struct 字段的**偏移是相对偏移**（相对于父 struct），而非绝对偏移。例如 `struct A { int x; }; struct B { A a; int y; };`，Definition 签名为 `@0[a]:record[s:4,a:4]{@0[x]:i32},@4[y]:i32`。内层 `A` 的 `@0[x]` 是相对于 A 自身的偏移，这是**正确**的——Definition 模式保留结构树，每个嵌套记录的偏移自然是相对的。✅ 无问题。

#### Scenario: Projection relationship
`definition_match ⟹ layout_match`

**推演验证**：
- Definition 包含字段名、继承树、限定名——任何两个 Definition 相同的类型，去掉字段名、展平继承和组合后，Layout 也应该相同。
- **潜在破坏点**: 如果 Definition 用的 size/align 和 Layout 用的 size/align 不一致。检查代码：两者都用 `sizeof(T)` 和 `alignof(T)`。✅ 一致。
- **潜在破坏点**: 多态标记不一致。Definition 用 `,polymorphic`，Layout 用 `,vptr`。两者都基于 `std::is_polymorphic_v<T>`。✅ 一致。
- **结论**: 投影关系成立。✅

### A2. Requirement: Signature Comparison

两个 `_match` 函数直接比较 `get_*_signature<T1>() == get_*_signature<T2>()`（`signature.hpp:31-33`, `43-45`）。CompileString 的 `operator==` 逐字符比较。✅ 正确。

### A3. Requirement: Layout Signature Flattening

已在 A1 中逐一验证。额外检查：

**边界情况: 空嵌套 struct**
`struct E {}; struct F { E e; int x; };`
展平时 `e` 是 class 类型，进入 `layout_all_prefixed`，但 E 无字段无基类，返回空串。
结果 F 签名只有 `@0:i32`。与 `struct G { int x; };` 相同。
F 和 G 的 `sizeof` 都是 4。✅ 正确。

**边界情况: 多重继承字段偏移**
`struct B1 { int a; }; struct B2 { int b; }; struct M : B1, B2 { int c; };`
B2 在 M 中的 offset 是 4（B1 占 4 字节）。代码 `layout_one_base_prefixed` 使用 `offset_of(base_info).bytes`。✅ 正确。

### A4. Requirement: Polymorphic Type Safety

✅ 已验证。`vptr` 标记在 `record[s:...,a:...,vptr]` 中。

### A5. Requirement: Qualified Names in Definition

✅ 已验证。`qualified_name_for` 递归走 `parent_of` 链。

**边界情况: 嵌套命名空间 `a::b::c::T`**
代码只递归到 grandparent 层就停止（`reflection_helpers.hpp:35-43`）。如果有三层以上嵌套命名空间：
```cpp
namespace a { namespace b { namespace c { struct T {}; } } }
```
- `R = ^^T`，`parent = c`，`grandparent = b`
- `is_namespace(grandparent) && has_identifier(grandparent)` → true
- 递归调用 `qualified_name_for<parent>()` 即 `qualified_name_for<^^c>()`
- 在那次调用中：`parent = b`, `grandparent = a`
- `is_namespace(a) && has_identifier(a)` → true
- 再递归 `qualified_name_for<^^b>()`
- 此时 `parent = a`, `grandparent = global namespace`
- `is_namespace(global) && has_identifier(global)` → **false**（全局命名空间无 identifier）
- 于是返回 `"a"`
- 回溯：`"a" + "::" + "b"` → `"a::b"`
- 回溯：`"a::b" + "::" + "c"` → `"a::b::c"`
- 回溯：`"a::b::c" + "::" + "T"` → `"a::b::c::T"` ✅

**结论**: 深层嵌套命名空间正确递归。✅

### A6. Requirement: Type Categories

#### Fundamental types
所有 `TypeSignature<int8_t>` 到 `TypeSignature<bool>` 的特化都返回固定字符串。✅

#### Record types
struct/class 都用 `record` 前缀。✅

#### Enum types
- Layout: `enum[s:SIZE,a:ALIGN]<underlying>` ✅
- Definition: `enum<QualifiedName>[s:SIZE,a:ALIGN]<underlying>` ✅

#### Union types
- `union[s:SIZE,a:ALIGN]{members}` ✅
- Layout 不展平成员 ✅

#### Array types
- `array[s:SIZE,a:ALIGN]<element,N>` ✅
- 字节数组归一化为 `bytes[s:N,a:1]` ✅

**正确性问题 C1**: `bytes[s:N,a:1]` 中 `N` 是数组**元素个数**而非字节数。但由于每个字节元素大小都是 1，`N` 恰好等于字节数。✅ 正确，但如果未来有 `char16_t` 被误加入 `is_byte_element_v`，就会出 bug。当前列表正确。

#### Bit-field types
- Layout: `@BYTE.BIT:bits<WIDTH,underlying>` ✅
- Definition: `@BYTE.BIT[NAME]:bits<WIDTH,underlying>` ✅

#### Anonymous members
- Definition: `<anon:N>` placeholder ✅ (`reflection_helpers.hpp:75-77`)

**测试覆盖缺失**: 当前测试中没有 bit-field、anonymous member、union 的 Definition 签名测试。

### A7. Requirement: Architecture Prefix
- 64-bit LE: `[64-le]` ✅
- 32-bit: `[32-le]`/`[32-be]` ✅ (代码中有但无法在 64-bit 平台测试)
- 平台相关类型: `long`, `wchar_t` 使用 `sizeof`/`alignof` 反映真实大小 ✅

### A8. Requirement: Empty Type Handling
- `struct Empty {}; struct WithEmpty : Empty { int x; double y; };` vs `struct Plain { int x; double y; };`
- 展平时 Empty 贡献空串，两者签名相同 ✅

### A9. Requirement: Alignment Support
- `alignas(16)` 体现在 `record[s:...,a:16]` ✅

---

## B. 发现的问题

### B1. Spec 遗漏：union Layout 不展平（P0 — spec 与代码不一致）

**现状**: 代码中 union 成员在 Layout 模式下**不展平**（使用 `get_layout_union_content`），但 spec 第 17-23 行（Layout signature scenario）未提及此行为。spec 只说了继承展平和组合展平。

**修复**: 在 spec 的 Layout signature scenario 中新增条款：
`- **AND** union members SHALL NOT be recursively flattened`

### B2. Spec 遗漏：Layout Signature Flattening 缺少 union 场景（P0）

**现状**: `Requirement: Layout Signature Flattening` 只有 4 个场景（继承、多级继承、组合、虚基类），但缺少 "union 成员不展平" 的场景。

**修复**: 新增场景：
```
#### Scenario: Union members not flattened
- **GIVEN** `struct Inner { int a; int b; }; union U { Inner x; double y; };`
- **WHEN** Layout signature is generated
- **THEN** union member `x` SHALL appear as complete record type signature
```

### B3. Spec 遗漏：投影关系的方向性说明（P1 — 文档精度）

**现状**: spec 第 35-38 行只说 `definition_match ⟹ layout_match`，但未明确说明反向不成立。用户可能误以为 `⟺`。

**修复**: 添加 NOTE：
`- **NOTE** The reverse does not hold: layout match does not imply definition match`

### B4. 测试覆盖不足：bit-field 和 anonymous member（P1）

**现状**: spec 要求支持 bit-field 和 anonymous member 签名格式，但 `test_two_layer.cpp` 中无相关测试。

**修复**: 添加 bit-field 和 anonymous member 的 static_assert 测试。

### B5. Spec 遗漏：Definition 的 enum 格式未精确描述（P2）

**现状**: spec 说 "enum types SHALL include their fully qualified name"（第 33 行），但未精确描述格式。实际 Definition 格式是 `enum<QualifiedName>[s:SIZE,a:ALIGN]<underlying>`（限定名在 `enum<>` 的尖括号内而非外部）。Layout 格式是 `enum[s:SIZE,a:ALIGN]<underlying>`（无限定名）。

**修复**: 在 Type Categories 的 Enum types 场景中精确描述两种格式。

### B6. 代码冗余：`get_field_offset` 未使用（P2）

**现状**: `reflection_helpers.hpp:61-65` 定义了 `get_field_offset<T, Index>()`，但代码中从未调用——所有偏移获取都在签名生成函数内部直接用 `offset_of(member).bytes`。

**修复**: 删除未使用的 `get_field_offset`。

---

## C. 修复方案总结

| # | 问题 | 优先级 | 修复 |
|---|------|--------|------|
| B1 | Spec 遗漏: union Layout 不展平 | P0 | 更新 spec Layout scenario |
| B2 | Spec 遗漏: 缺少 union 展平场景 | P0 | 新增 spec scenario |
| B3 | Spec 遗漏: 投影关系方向性 | P1 | 添加 NOTE |
| B4 | 测试缺失: bit-field / anonymous | P1 | 添加 static_assert 测试 |
| B5 | Spec 遗漏: enum Definition 格式 | P2 | 精确描述 |
| B6 | 代码冗余: `get_field_offset` | P2 | 删除 |
