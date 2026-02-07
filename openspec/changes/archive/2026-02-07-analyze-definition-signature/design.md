## Context

本文档是 Definition 签名（第二层）的系统性正确性和完备性审计报告。
审计基于 `signature_detail.hpp` 源码、`signature` spec 以及 `test_two_layer.cpp` 测试用例。

## 审计方法

对 Definition 签名的每个编码维度逐项审查：
1. **格式正确性**：生成的签名格式是否与 spec 一致
2. **区分能力**：结构不同的类型对是否产生不同签名
3. **等价能力**：结构相同的类型对是否产生相同签名
4. **投影关系**：`def_match(T,U) ⟹ layout_match(T,U)` 是否恒成立

---

## 1. 编码维度逐项审计

### 1.1 record 类型 — ✅ 正确

**格式**: `record[s:SIZE,a:ALIGN]{CONTENT}` 或 `record[s:SIZE,a:ALIGN,polymorphic]{CONTENT}`

**审计**:
- size 和 alignment 编码自 `sizeof(T)` / `alignof(T)` — 正确
- `polymorphic` 标记通过 `std::is_polymorphic_v<T>` 检测 — 正确
- 内容由 `definition_content<T>()` 生成，先基类再字段 — 正确

**签名示例**:
```
record[s:16,a:8]{@0[x]:i32[s:4,a:4],@8[y]:f64[s:8,a:8]}
```

### 1.2 字段签名 — ✅ 正确

**格式**: `@OFFSET[NAME]:TYPE_SIG`

**审计**:
- 偏移量使用 `offset_of(member).bytes` — 正确（编译器验证的偏移）
- 字段名使用 `identifier_of(member)` — 正确
- 匿名字段使用 `<anon:INDEX>` — 正确
- 字段类型递归调用 `TypeSignature<FieldType, SignatureMode::Definition>::calculate()` — 正确

### 1.3 位域签名 — ✅ 正确

**格式**: `@BYTE.BIT[NAME]:bits<WIDTH,TYPE_SIG>`

**审计**:
- 字节偏移 + 位偏移 使用 `offset_of(member).bytes` / `.bits` — 正确
- 位宽使用 `bit_size_of(member)` — 正确
- 底层类型正确递归 — 正确

### 1.4 继承 — ✅ 正确

**格式**: `~base<QUALIFIED_NAME>:TYPE_SIG` / `~vbase<QUALIFIED_NAME>:TYPE_SIG`

**审计**:
- 普通继承使用 `~base<>` 前缀 — 正确
- 虚继承使用 `~vbase<>` 前缀 — 正确
- 基类名使用 `qualified_name_for<type_of(base_info)>()` — 正确
- 基类内容递归调用 `TypeSignature<BaseType, Definition>::calculate()` — 正确
- 基类在字段之前输出（`definition_content` 先 bases 再 fields）— 正确

### 1.5 枚举类型 — ✅ 正确

**格式**: `enum<QUALIFIED_NAME>[s:SIZE,a:ALIGN]<UNDERLYING_SIG>`

**审计**:
- Definition 模式包含限定名 — 正确
- Layout 模式不包含限定名 — 正确
- 底层类型递归 — 正确

### 1.6 union 类型 — ✅ 正确

**格式**: `union[s:SIZE,a:ALIGN]{FIELDS}`

**审计**:
- Definition 模式使用 `definition_fields<T>()` — 正确（包含字段名）
- Layout 模式使用 `get_layout_union_content<T>()` — 正确（不含字段名）
- union 成员不递归展平 — 正确

### 1.7 限定名构建 — ⚠️ 有深度限制

**实现**: `qualified_name_for<R>()` 递归 `parent_of` 链

**审计**:
- 当前实现只向上递归到**祖父级**命名空间（grandparent）
- 对于深度 ≤ 2 的命名空间（如 `ns1::ns2::Type`）完全正确
- 对于深度 ≥ 3 的命名空间（如 `a::b::c::Type`）会**截断**——只输出 `b::c::Type` 而非 `a::b::c::Type`

**影响**: 
- 如果两个类型在不同的顶级命名空间下但中间层命名空间和名字相同，签名可能**错误匹配**
- 例如: `org1::common::Tag` vs `org2::common::Tag` — 两者签名均为 `common::Tag`，产生碰撞

**严重程度**: 🟡 中等 — 极深的命名空间在实际 C++ 代码中不常见，但确实是一个正确性缺陷

### 1.8 数组类型 — ✅ 正确

**格式**: `array[s:SIZE,a:ALIGN]<ELEMENT_SIG,N>` 或 `bytes[s:N,a:1]`

**审计**:
- 数组 Definition 签名与 Layout 签名在格式上完全相同（两者都不含字段名，因为数组没有字段名）
- 字节数组归一化正确

### 1.9 CV 限定和指针/引用类型 — ✅ 正确

**审计**:
- `const T` / `volatile T` / `const volatile T` 都剥除并转发到 `TypeSignature<T, Mode>` — 正确
- 指针 `ptr`、引用 `ref`、右值引用 `rref`、成员指针 `memptr` — 均正确
- Definition 与 Layout 对这些类型无差异（指针类型没有内部结构）— 正确

### 1.10 函数指针类型 — ✅ 正确

**审计**:
- 所有变体（普通/noexcept/variadic）都归一化为 `fnptr[s:SIZE,a:ALIGN]` — 正确

---

## 2. 投影关系验证

**命题**: `def_match(T,U) ⟹ layout_match(T,U)`

### 2.1 系统性论证

Definition 签名是 Layout 签名的**信息超集**。具体来说：

| 维度 | Layout 编码 | Definition 编码 | Definition ⊇ Layout? |
|------|-------------|----------------|----------------------|
| size | ✅ | ✅ | ✅ |
| alignment | ✅ | ✅ | ✅ |
| 字段偏移 | ✅ (绝对) | ✅ (相对) | ⚠️ 见 2.2 |
| 字段类型 | ✅ | ✅ | ✅ |
| 字段名 | ❌ | ✅ | ✅ |
| 继承 | ❌ (展平) | ✅ (保留树) | ✅ |
| 限定名 | ❌ | ✅ | ✅ |
| 多态标记 | `vptr` | `polymorphic` | ⚠️ 见 2.3 |
| 组合展平 | ✅ (展平) | ❌ (保留树) | ⚠️ 见 2.4 |

### 2.2 偏移量差异 — ⚠️ 需要关注

- **Layout**: 组合被展平后，所有偏移量都是**全局绝对偏移**
- **Definition**: 嵌套 struct 保留树状结构，字段偏移是**相对于自身 record 的局部偏移**

这意味着：
```cpp
struct Inner { int32_t a; int32_t b; };
struct Outer1 { int32_t pad; Inner inner; };
struct Outer2 { int32_t pad; Inner inner; };
```

- Layout: `@0:i32,@4:i32,@8:i32` — 展平为全局偏移
- Definition Outer1: `@0[pad]:i32,@4[inner]:record{@0[a]:i32,@4[b]:i32}` — Inner 内部偏移是 0, 4

**投影关系**: 如果 `def_sig(Outer1) == def_sig(Outer2)`，则它们的内部结构完全相同（相同的嵌套层次、相同的局部偏移），因此 Layout 展平后也一定相同。✅ **投影关系成立**。

### 2.3 多态标记映射 — ✅ 正确

- Layout 使用 `vptr`，Definition 使用 `polymorphic`
- 两者都通过 `std::is_polymorphic_v<T>` 判断
- 如果 Definition 匹配，则两个类型的多态性一致，Layout 的 vptr 标记也一致

### 2.4 组合展平差异 — ✅ 正确

- Definition 签名中嵌套 struct 保留为 `record{...}`
- 如果两个类型的 Definition 签名完全相同，则它们的嵌套结构完全一致
- 展平后的 Layout 也一定相同（因为相同的嵌套 record 展平为相同的叶字段序列）

**结论**: 投影关系在所有已分析场景下成立。✅

---

## 3. 发现的问题

### 🔴 P0: qualified_name_for 的深度截断

**问题**: `qualified_name_for` 只递归到 grandparent，超过 2 层的命名空间路径会被截断。

**代码位置**: `signature_detail.hpp:25-45`

**影响**: 可能导致 false positive（不同类型产生相同 Definition 签名）

**示例**:
```cpp
namespace org1 { namespace common { struct Tag { int id; }; } }
namespace org2 { namespace common { struct Tag { int id; }; } }
// 当前: qualified_name = "common::Tag" for both → 签名碰撞!
// 应该: "org1::common::Tag" vs "org2::common::Tag"
```

**修复建议**: 将 `qualified_name_for` 改为完全递归（递归直到到达全局命名空间），而非只检查两级。

### 🟡 P1: Definition 签名中 record 不包含类型自身的限定名

**观察**: Definition 签名对 struct/class 类型不编码类型自身的限定名。

**当前行为**:
```cpp
namespace ns1 { struct Point { int32_t x; int32_t y; }; }
namespace ns2 { struct Point { int32_t x; int32_t y; }; }
// Definition: 两者都是 record[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}
// → definition_signatures_match 返回 true
```

**设计分析**: 这是 **有意的设计选择**（Structural Analysis vs Nominal Analysis），在 `project.md` 和 spec 中均有明确记录。TypeLayout 回答的问题是 "两个类型的结构是否等价" 而非 "它们是否是同一个类型"。

**结论**: ✅ **不是缺陷**，是设计哲学的体现。但应确保用户文档清楚说明这一行为。

### 🟡 P2: union 的 Definition 签名不包含成员名（在 Layout 层面）

**观察**: 在 Definition 模式下，union 使用 `definition_fields<T>()`，正确包含字段名。
在 Layout 模式下，union 使用 `get_layout_union_content<T>()`，不包含字段名。
两者行为一致且正确。

**结论**: ✅ **无问题**。

### 🟢 P3: 缺少多重继承 + 命名空间的 Definition 测试

**观察**: 当前测试覆盖了：
- 单继承 Definition（`~base<>` 存在性检查）
- 不同命名空间基类的区分（`test_base_collision`）
- 多级继承 Layout 展平

**但缺少**:
- 多重继承的 Definition 签名格式验证
- 虚继承的 Definition 签名中 `~vbase<>` 格式验证
- 多级继承的 Definition 签名树结构验证

---

## 4. 测试覆盖完备性评估

| Definition 编码维度 | 有测试? | 测试类型 |
|---------------------|---------|----------|
| record 基本格式 | ✅ | 精确匹配 |
| 字段名编码 | ✅ | 差异测试 (Simple vs Simple2) |
| 继承 `~base<>` | ✅ | 存在性检查 |
| 虚继承 `~vbase<>` | ❌ | 无 Definition 测试 |
| 多态 `polymorphic` | ✅ | 存在性检查 |
| 枚举限定名区分 | ✅ | 差异测试 (Color vs Shape) |
| 命名空间基类区分 | ✅ | 差异测试 (ns1::Tag vs ns2::Tag) |
| 位域字段名 | ✅ | 存在性检查 ([a], [b]) |
| 匿名成员 | ✅ | 存在性检查 (<anon:) |
| 投影关系 | ✅ | 逻辑验证 |
| 多重继承 Definition 格式 | ❌ | 无 |
| 深层嵌套 struct Definition | ❌ | 无 |
| union Definition 字段名 | ❌ | 无 |
| 深层命名空间 (≥3级) | ❌ | 无 |

---

## 5. 改进建议优先级

| 优先级 | 问题 | 建议 | 风险 |
|--------|------|------|------|
| 🔴 P0 | `qualified_name_for` 深度截断 | 改为完全递归 | 正确性缺陷 — 可导致签名碰撞 |
| 🟢 P3 | 测试缺少 `~vbase<>` / 多重继承 / union 字段名 / 深层命名空间验证 | 补充测试用例 | 无代码修改，仅增加测试覆盖 |

---

## 6. 总结

Definition 签名系统整体设计**正确且符合 spec**，投影关系在所有分析场景下成立。

发现 **1 个正确性缺陷**（P0: 命名空间深度截断）和 **1 个测试覆盖缺口**（P3: 4 个维度缺少测试）。

P0 缺陷需要作为后续修复提案处理。P3 可以作为测试补强任务纳入。
