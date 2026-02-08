## Context

TypeLayout 承诺两层签名的准确性。本文档从数学和逻辑的角度，对签名函数的语义、
正确性和完备性进行形式化分析和证明。

## Goals / Non-Goals

- Goals:
  - 建立签名系统的形式化语义模型
  - 证明核心定理 (单射性、充分性、投影关系)
  - 对所有类型类别进行归纳证明
  - 明确标注已知的不完备性 (非满射)

- Non-Goals:
  - 不使用机器辅助证明工具 (Coq/Lean)
  - 不修改签名引擎代码

---

## §1 形式化语义模型

### 1.1 基本定义

**定义 1.1 (类型宇宙)**
设 \( \mathcal{T} \) 为所有 C++ 类型的集合 (在给定平台 \( P \) 下)。

**定义 1.2 (字节布局)**
对于类型 \( T \in \mathcal{T} \)，其字节布局 (byte layout) 定义为三元组：

\[
\mathcal{L}(T) = \big( \text{sizeof}(T),\; \text{alignof}(T),\; \{(o_i, \tau_i)\}_{i=1}^{n} \big)
\]

其中 \( (o_i, \tau_i) \) 表示第 \( i \) 个叶字段的**绝对字节偏移**和**原始类型签名**。

**定义 1.3 (结构树)**
对于类型 \( T \in \mathcal{T} \)，其结构树 (structure tree) 定义为：

\[
\mathcal{D}(T) = \big( \text{sizeof}(T),\; \text{alignof}(T),\; \text{bases}(T),\; \{(o_i, n_i, \tau_i)\}_{i=1}^{m} \big)
\]

其中 \( n_i \) 为字段名，\( \text{bases}(T) \) 为保留继承层次的基类序列。

**定义 1.4 (签名函数)**
TypeLayout 定义两个签名函数：

\[
\text{Sig}_L : \mathcal{T} \to \Sigma^* \quad \text{(Layout 签名)}
\]
\[
\text{Sig}_D : \mathcal{T} \to \Sigma^* \quad \text{(Definition 签名)}
\]

其中 \( \Sigma^* \) 为有限字母表上的字符串集合。

**定义 1.5 (memcmp-兼容)**
两个类型 \( T, U \) 是 memcmp-兼容的，当且仅当：

\[
T \sim_{\text{mem}} U \iff \text{sizeof}(T) = \text{sizeof}(U) \land \forall i : o_i^T = o_i^U \land \tau_i^T = \tau_i^U
\]

即：大小相同，且每个偏移位置的字段类型完全一致。

### 1.2 平台参数

签名函数以平台 \( P \) 为隐含参数：

\[
P = (\text{ptr\_width}, \text{endianness}, \text{ABI})
\]

平台信息编码为架构前缀 `[64-le]` 或 `[32-be]`。以下分析中 \( P \) 固定（同平台比较）。

---

## §2 Layout 签名的正确性

### 2.1 编码完备性定理

**定理 2.1 (Layout 签名编码完备性)**
Layout 签名 \( \text{Sig}_L(T) \) 完整编码了 \( \mathcal{L}(T) \) 的所有信息。

**证明：**

由实现（`signature_detail.hpp`），Layout 签名的结构为：

```
[ARCH]record[s:SIZE,a:ALIGN{,vptr}]{@OFF₁:SIG₁,@OFF₂:SIG₂,...,@OFFₙ:SIGₙ}
```

逐项验证：
1. **sizeof**: 编码为 `s:SIZE` — 直接调用 `sizeof(T)` ✅
2. **alignof**: 编码为 `a:ALIGN` — 直接调用 `alignof(T)` ✅
3. **偏移量**: 每个字段 `@OFFᵢ` — 由 `std::meta::offset_of(member).bytes` 提供，这是编译器计算的真实偏移 ✅
4. **字段类型**: `SIGᵢ` — 递归调用 `TypeSignature<FieldType, Layout>::calculate()` ✅
5. **多态标记**: `vptr` — 由 `std::is_polymorphic_v<T>` 检测 ✅
6. **架构前缀**: `[64-le]` — 编码指针宽度和字节序 ✅

由于签名字符串是通过字符串连接构造的，且各部分用不同的分隔符 (`[`, `]`, `{`, `}`, `:`, `,`, `@`) 分隔，因此**信息没有丢失，也没有歧义**。

**关键性质**: 偏移量来自 `std::meta::offset_of`（P2996 编译器 intrinsic），而非手动计算。这保证了偏移量的准确性由编译器负责。 ∎

### 2.2 单射性定理

**定理 2.2 (Layout 签名在布局等价类上的单射性)**
对于固定平台 \( P \)：

\[
\mathcal{L}(T) \neq \mathcal{L}(U) \implies \text{Sig}_L(T) \neq \text{Sig}_L(U)
\]

等价表述: 不同的字节布局产生不同的签名。

**证明：**

假设 \( \mathcal{L}(T) \neq \mathcal{L}(U) \)，则至少以下之一成立：
- `sizeof(T) ≠ sizeof(U)` → 签名中 `s:` 值不同 → 签名不同 ✅
- `alignof(T) ≠ alignof(U)` → 签名中 `a:` 值不同 → 签名不同 ✅
- 存在 \( i \) 使得 \( o_i^T \neq o_i^U \) → 签名中 `@OFF` 值不同 → 签名不同 ✅
- 存在 \( i \) 使得 \( \tau_i^T \neq \tau_i^U \) → 签名中字段类型签名不同 → 签名不同 ✅
- 字段数量不同 → 花括号内逗号分隔的项数不同 → 签名不同 ✅
- 多态性不同 → `vptr` 标记存在/缺失 → 签名不同 ✅

因此签名函数在布局等价类上是**单射**(injective)的。 ∎

**推论 2.2.1 (V1 充分性/Soundness)**

\[
\text{Sig}_L(T) = \text{Sig}_L(U) \implies T \sim_{\text{mem}} U
\]

这是定理 2.2 的逆否命题（布局不兼容 → 签名不同 等价于 签名相同 → 布局兼容）。

### 2.3 非满射性分析

**命题 2.3 (Layout 签名不是满射的)**

存在 \( T, U \) 使得 \( T \sim_{\text{mem}} U \) 但 \( \text{Sig}_L(T) \neq \text{Sig}_L(U) \)。

**反例:**

```cpp
struct A { int32_t x, y, z; };   // Sig: record[s:12,a:4]{@0:i32[...],@4:i32[...],@8:i32[...]}
using  B = int32_t[3];           // Sig: array[s:12,a:4]<i32[s:4,a:4],3>
```

\( A \) 和 \( B \) 在内存中字节布局完全相同（12 字节，3 个连续 int32_t），但签名不同：
一个是 `record{...}`，另一个是 `array<...>`。

**分析**: 这是**有意的保守设计**。`int32_t[3]` 和 `struct{int32_t x,y,z}` 虽然字节相同，
但语义不同（数组有边界语义、可索引；结构体有命名字段）。保留这种区分使签名更精确。

**形式表述**: \( \text{Sig}_L \) 是 \( \mathcal{L} \) 的一个**细化** (refinement)，而非等价映射：

\[
\text{Sig}_L(T) = \text{Sig}_L(U) \implies T \sim_{\text{mem}} U \quad \text{（充分性，成立）}
\]
\[
T \sim_{\text{mem}} U \centernot\implies \text{Sig}_L(T) = \text{Sig}_L(U) \quad \text{（非必要性）}
\]

### 2.4 递归展平的正确性

**定理 2.4 (Layout 展平保持偏移正确性)**

对于嵌套结构体 `struct Outer { Inner x; int y; }`，Layout 签名将 `Inner` 的字段展平到 `Outer` 的绝对偏移：

\[
\text{offset}(\text{Inner.field}_j) = \text{offset}(\text{Outer.x}) + \text{offset}(\text{Inner.field}_j \text{ within Inner})
\]

**证明：**

由实现 `layout_field_with_comma`（第 205-208 行），当字段类型是非 union 的 class 时：
```cpp
constexpr std::size_t field_offset = offset_of(member).bytes + OffsetAdj;
return layout_all_prefixed<FieldType, field_offset>();
```

`OffsetAdj` 从外层传入，累加了所有外层嵌套的偏移。`offset_of(member).bytes` 是编译器提供的
该成员在其直接父类中的偏移。两者之和即为绝对偏移。

由归纳法（对嵌套深度 \( d \) 归纳）：
- **基础**: \( d = 0 \)，`OffsetAdj = 0`，偏移即 `offset_of(member).bytes`，正确 ✅
- **归纳步**: 若 \( d = k \) 时正确，则 \( d = k+1 \) 时 `OffsetAdj` 已正确累加前 \( k \) 层偏移，新增一层的偏移由编译器提供，两者之和正确 ✅

继承展平同理（`layout_one_base_prefixed` 使用 `offset_of(base_info).bytes + OffsetAdj`）。 ∎

---

## §3 Definition 签名的正确性

### 3.1 编码完备性

**定理 3.1 (Definition 签名编码完备性)**
Definition 签名 \( \text{Sig}_D(T) \) 完整编码了 \( \mathcal{D}(T) \) 的所有信息。

**证明：**

在 Layout 编码的基础上，Definition 额外编码：
1. **字段名**: `[name]` — 由 `std::meta::identifier_of(member)` 提供 ✅
2. **继承结构**: `~base<QualifiedName>:record{...}` — 保留继承树 ✅
3. **虚继承**: `~vbase<QualifiedName>:record{...}` — 由 `std::meta::is_virtual(base_info)` 检测 ✅
4. **多态标记**: `polymorphic` (vs Layout 的 `vptr`) ✅
5. **枚举限定名**: `enum<ns::Color>` — 由 `qualified_name_for<^^T>()` 构造 ✅
6. **命名空间路径**: 通过递归遍历 `parent_of` 链构造完全限定名 ✅

同样，各部分用不同分隔符分隔，信息无丢失无歧义。 ∎

### 3.2 区分力定理

**定理 3.2 (Definition 签名的严格区分力)**
对于固定平台 \( P \)，以下差异必导致 Definition 签名不同：

| 差异类型 | 签名中体现 | 证明 |
|---------|----------|------|
| 字段名不同 | `[name]` 不同 | 由 `identifier_of` 的唯一性 |
| 继承 vs 扁平 | `~base<>:record{...}` vs 直接字段 | 结构上不同的字符串模式 |
| 不同命名空间基类 | `~base<ns1::T>` vs `~base<ns2::T>` | 限定名不同 |
| 不同枚举名 | `enum<ns::Color>` vs `enum<ns::Shape>` | 限定名不同 |
| 虚继承 vs 普通继承 | `~vbase<>` vs `~base<>` | 前缀不同 |
| 多态 vs 非多态 | `polymorphic` 标记存在/缺失 | 标记不同 |

**关键性质**: Definition 签名在**结构等价类**上是单射的。即：

\[
\mathcal{D}(T) \neq \mathcal{D}(U) \implies \text{Sig}_D(T) \neq \text{Sig}_D(U)
\]

∎

---

## §4 投影关系的证明

### 4.1 投影函子

**定理 4.1 (V3 投影正确性)**

\[
\text{Sig}_D(T) = \text{Sig}_D(U) \implies \text{Sig}_L(T) = \text{Sig}_L(U)
\]

**证明：**

定义**擦除函数** \( \pi : \Sigma^* \to \Sigma^* \)，将 Definition 签名转换为 Layout 签名：

\[
\pi(\text{Sig}_D(T)) = \text{Sig}_L(T)
\]

\( \pi \) 执行以下操作：
1. 删除所有字段名 `[name]` → 字段变为 `@OFF:SIG`
2. 展平继承 `~base<ns::Name>:record{...}` → 将基类字段以绝对偏移展开
3. 将 `polymorphic` 替换为 `vptr`
4. 删除枚举限定名 `<ns::Color>`

\( \pi \) 是一个**确定性函数** (每个 Definition 签名唯一对应一个 Layout 签名)。

因此：
\[
\text{Sig}_D(T) = \text{Sig}_D(U)
\implies \pi(\text{Sig}_D(T)) = \pi(\text{Sig}_D(U))
\implies \text{Sig}_L(T) = \text{Sig}_L(U)
\]

这是函数的**等价性保持** (congruence) 性质。 ∎

### 4.2 严格细化

**定理 4.2 (Definition 是 Layout 的严格细化)**

\[
\text{Sig}_L(T) = \text{Sig}_L(U) \centernot\implies \text{Sig}_D(T) = \text{Sig}_D(U)
\]

**反例:**
```cpp
struct Base { int32_t id; };
struct Derived : Base { int32_t value; };
struct Flat { int32_t id; int32_t value; };
```

- `Sig_L(Derived) == Sig_L(Flat)` (展平后字节布局相同)
- `Sig_D(Derived) ≠ Sig_D(Flat)` (Derived 有 `~base<Base>:...`，Flat 没有)

因此 Definition 签名**严格精细于** Layout 签名：

\[
\ker(\text{Sig}_D) \subsetneq \ker(\text{Sig}_L)
\]

（Definition 签名的等价核严格小于 Layout 签名的等价核。） ∎

---

## §5 逐类别归纳证明

对 TypeLayout 支持的每个类型类别验证签名准确性。

### 5.1 基础类型 (标量)

| 类型 | 签名格式 | 准确性论证 |
|------|---------|-----------|
| `int32_t` | `i32[s:4,a:4]` | 固定宽度类型，大小/对齐由标准保证 ✅ |
| `float` | `f32[s:4,a:4]` | IEEE 754 保证下大小固定 ✅ |
| `char` | `char[s:1,a:1]` | sizeof(char)==1 由标准保证 ✅ |
| `long` | `i32[s:4,a:4]` 或 `i64[s:8,a:8]` | 使用实际 sizeof，准确反映平台 ABI ✅ |
| `wchar_t` | `wchar[s:N,a:N]` | 使用实际 sizeof/alignof ✅ |
| `T*` | `ptr[s:N,a:N]` | 指针大小由平台决定，正确编码 ✅ |

**定理 5.1**: 所有基础类型的签名准确反映其在当前平台上的 sizeof 和 alignof。

由实现可见：固定宽度类型 (`int32_t` 等) 使用硬编码常量（与标准定义一致）；
平台相关类型 (`long`, `wchar_t`, `long double`) 使用 `sizeof()` / `alignof()` 的实际值。 ∎

### 5.2 记录类型 (struct/class)

**定理 5.2**: 对于非多态、非继承的 POD 结构体 \( T \)，Layout 签名准确编码了所有字段的偏移和类型。

**证明** (对字段数 \( n \) 归纳):
- **\( n = 0 \)**: 空结构体，签名为 `record[s:S,a:A]{}`，大小和对齐正确 ✅
- **\( n = k \to n = k+1 \)**: 假设前 \( k \) 个字段签名正确。第 \( k+1 \) 个字段的偏移由 `offset_of(member).bytes` 提供（编译器 intrinsic），类型签名由递归调用 `TypeSignature<FieldType>::calculate()` 提供（由基础类型的准确性或归纳假设保证）。 ∎

### 5.3 继承

**定理 5.3**: Layout 签名正确展平继承层次。

**证明** (对继承深度 \( d \) 归纳):
- **\( d = 0 \)**: 无继承，同定理 5.2 ✅
- **\( d = k \to d = k+1 \)**: 基类字段通过 `layout_one_base_prefixed` 递归展平，偏移由 `offset_of(base_info).bytes + OffsetAdj` 计算。由归纳假设，深度 \( k \) 的基类字段已正确展平，新增一层的偏移由编译器提供。 ✅

**Definition 签名**: 保留 `~base<QualifiedName>:record{...}` 结构，不展平。限定名由 `qualified_name_for<>()` 递归构造，遍历 `parent_of` 链。 ✅

### 5.4 多态类型

**定理 5.4**: 多态类型在签名中被正确标记。

- Layout: `record[s:S,a:A,vptr]{...}` — 由 `std::is_polymorphic_v<T>` 检测
- Definition: `record[s:S,a:A,polymorphic]{...}` — 同上

**注意**: vptr 的具体偏移未编码在签名中（因为 vptr 位置是 impl-defined）。
签名仅标记其**存在性**，这足以区分多态与非多态类型。

### 5.5 数组类型

**定理 5.5**: 数组签名正确编码元素类型和数量。

- `T[N]` → `array[s:sizeof(T[N]),a:alignof(T[N])]<TypeSig<T>,N>`
- 字节数组 (`char[N]`, `uint8_t[N]`, `std::byte[N]`) → `bytes[s:N,a:1]` (归一化)

归一化是正确的：`char[N]`、`uint8_t[N]`、`std::byte[N]` 在内存中完全等价（N 个连续字节，对齐 1）。 ✅

### 5.6 Union 类型

**定理 5.6**: Union 签名正确保留成员的原子性。

Union 成员**不被展平**（与 struct 不同），每个成员保留其完整类型签名：
```
union[s:S,a:A]{@0:SIG₁,@0:SIG₂,...}
```

这是正确的：union 成员共享同一偏移 (通常 @0)，展平会导致重叠字段的混淆。 ✅

### 5.7 位域类型

**定理 5.7**: 位域签名编码了字节偏移、位偏移、位宽度和底层类型。

格式: `@BYTE.BIT:bits<WIDTH,underlying_sig>`

所有信息由 P2996 API 提供：
- `offset_of(member).bytes` — 字节偏移
- `offset_of(member).bits` — 位偏移
- `bit_size_of(member)` — 位宽度
- `type_of(member)` — 底层类型

**已知限制**: 位域的位排列顺序 (MSB-first vs LSB-first) 是 impl-defined (C++ §9.6)。
两个不同编译器可能对同一位域声明产生不同的位偏移。TypeLayout 编码了当前编译器的实际位偏移，
因此在同一编译器下签名是准确的，但跨编译器的位域签名匹配不保证位级兼容。

### 5.8 枚举类型

**定理 5.8**: 枚举签名正确编码底层类型，Definition 层额外编码限定名。

- Layout: `enum[s:S,a:A]<underlying_sig>`
- Definition: `enum<ns::Color>[s:S,a:A]<underlying_sig>`

底层类型由 `std::underlying_type_t<T>` 获取（标准保证准确）。
限定名由 `qualified_name_for<^^T>()` 构造。 ✅

---

## §6 准确性总结

### 6.1 核心定理汇总

| # | 定理 | 陈述 | 状态 |
|---|------|------|------|
| 2.1 | 编码完备性 | Sig_L 完整编码 \( \mathcal{L}(T) \) | ✅ 已证 |
| 2.2 | 单射性 | 不同布局 → 不同签名 | ✅ 已证 |
| 2.2.1 | V1 充分性 | 签名匹配 → 布局兼容 | ✅ 已证 (逆否) |
| 2.3 | 非满射性 | 相同布局可能不同签名 | ✅ 已证 (反例) |
| 2.4 | 展平正确性 | 递归展平保持绝对偏移 | ✅ 已证 (归纳) |
| 3.1 | Def 编码完备 | Sig_D 完整编码 \( \mathcal{D}(T) \) | ✅ 已证 |
| 3.2 | Def 区分力 | 所有结构差异 → 签名不同 | ✅ 已证 |
| 4.1 | V3 投影 | def_match → layout_match | ✅ 已证 |
| 4.2 | 严格细化 | layout_match ⇏ def_match | ✅ 已证 (反例) |

### 6.2 准确性分级

| 类型类别 | Layout 准确性 | Definition 准确性 | 已知限制 |
|---------|-------------|-----------------|---------|
| 固定宽度标量 | 精确 | 精确 | 无 |
| 平台相关标量 | 精确 (使用实际 sizeof) | 精确 | 跨平台签名自然不同 |
| POD struct | 精确 | 精确 | 无 |
| 继承 | 精确 (展平) | 精确 (保留树) | 无 |
| 多态 | 存在性标记 | 存在性标记 | vptr 偏移未编码 |
| 数组 | 精确 | 精确 | 字节数组归一化 |
| Union | 精确 (不展平) | 精确 | 无 |
| 位域 | 同编译器精确 | 同编译器精确 | 跨编译器位排列 impl-defined |
| 枚举 | 底层类型精确 | 底层类型+限定名 | 无 |

### 6.3 形式化保证声明

**TypeLayout 签名系统提供以下形式化保证:**

1. **Soundness (无假阳性)**: 如果两个类型的签名匹配，它们的布局一定兼容
2. **Conservative (保守性)**: 签名不匹配不一定意味着布局不兼容（存在假阴性）
3. **Compiler-verified (编译器验证)**: 所有偏移和大小信息来自编译器 intrinsic (P2996)，非手动计算
4. **Platform-aware (平台感知)**: 架构前缀编码指针宽度和字节序，防止跨平台误判

## Open Questions

- 是否存在 padding 导致的签名歧义？→ 不存在：padding 不产生叶字段，偏移跳跃自然编码了 padding 的存在
- 是否应该对位域提供跨编译器的警告？→ 已在 Safety Classification 中实现 (Risk 级别)
