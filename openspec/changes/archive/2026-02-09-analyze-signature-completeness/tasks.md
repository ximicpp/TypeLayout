## 1. 完备性定义
- [x] 1.1 定义 Layout 层完备性的精确含义
- [x] 1.2 定义 Definition 层完备性的精确含义

## 2. Layout 层逐类型审计
- [x] 2.1 基本类型（整数、浮点、字符、bool）
- [x] 2.2 指针与引用类型
- [x] 2.3 结构体/类（非多态）
- [x] 2.4 继承展平
- [x] 2.5 多态类型
- [x] 2.6 Union
- [x] 2.7 Enum
- [x] 2.8 数组
- [x] 2.9 位域
- [x] 2.10 CV 限定符处理

## 3. Definition 层逐类型审计
- [x] 3.1 字段名编码
- [x] 3.2 继承结构保留
- [x] 3.3 枚举限定名
- [x] 3.4 嵌套结构保留

## 4. 已知不完备项分析
- [x] 4.1 列举所有已知不完备项
- [x] 4.2 逐项分析合理性

## 5. 签名语法无歧义性
- [x] 5.1 分析签名字符串的解析唯一性

## 6. 综合评估
- [x] 6.1 完备性总评
- [x] 6.2 发现的问题与建议

---

## 7. 分析结果

### 7.1 完备性定义

**Layout 完备性**：对于同一平台 P 上的任意两个类型 T, U：
```
L_P(T) ≠ L_P(U) ⟹ ⟦T⟧_L ≠ ⟦U⟧_L
```
即：字节布局不同的类型必须产生不同的 Layout 签名（无漏报）。

**Definition 完备性**：对于同一平台 P 上的任意两个类型 T, U：
```
D_P(T) ≠ D_P(U) ⟹ ⟦T⟧_D ≠ ⟦U⟧_D
```
即：结构属性不同的类型必须产生不同的 Definition 签名（无漏报）。

**注意**：完备性是"无漏报"（injectivity），不是"无误报"。误报方向（相同布局产生不同签名）
是保守安全的。PROOFS.md 中的 Theorem 3.1 (Encoding Faithfulness) 证明的是无误报（soundness），
这里分析的是无漏报（completeness/injectivity）。

### 7.2 Layout 层逐类型审计

#### 7.2.1 基本类型

| 类型 | 签名格式 | 编码的信息 | 完备性 |
|------|---------|-----------|:------:|
| `int8_t` | `i8[s:1,a:1]` | kind + size + align | ✅ |
| `uint32_t` | `u32[s:4,a:4]` | kind + size + align | ✅ |
| `float` | `f32[s:4,a:4]` | kind + size + align | ✅ |
| `double` | `f64[s:8,a:8]` | kind + size + align | ✅ |
| `long double` | `f80[s:N,a:M]` | kind + size + align（平台相关） | ✅ |
| `char` | `char[s:1,a:1]` | kind + size + align | ✅ |
| `bool` | `bool[s:1,a:1]` | kind + size + align | ✅ |
| `wchar_t` | `wchar[s:N,a:M]` | kind + size + align（平台相关） | ✅ |

**关键设计决策：类型别名去重**

代码使用 `requires (!std::is_same_v<long, int64_t>)` 确保同一底层类型只有一个特化。
这是正确的——`int64_t` 和 `long` 在 LP64 平台上是同一类型，只需一个签名。

**完备性判定：✅ 完备**

每个基本类型通过 `kind_prefix[s:SIZE,a:ALIGN]` 三元组唯一标识。
`kind` 前缀区分语义（`i` vs `u` vs `f` vs `char`），`size` 和 `align` 区分布局。

**潜在问题：`signed char` vs `int8_t`**

在大多数平台上 `signed char` 就是 `int8_t`，代码通过 `requires` 约束处理。
但如果某平台上它们不同（理论上可能），两者会产生相同的签名 `i8[s:1,a:1]`。
这是**合理的**——它们字节布局确实相同，签名相同是正确行为。

#### 7.2.2 指针与引用类型

| 类型 | 签名格式 | 编码的信息 | 完备性 |
|------|---------|-----------|:------:|
| `T*` | `ptr[s:8,a:8]` | kind + size + align | ⚠️ |
| `T&` | `ref[s:8,a:8]` | kind + size + align | ⚠️ |
| `T&&` | `rref[s:8,a:8]` | kind + size + align | ⚠️ |
| `T C::*` | `memptr[s:N,a:M]` | kind + size + align | ⚠️ |
| `R(*)(Args...)` | `fnptr[s:8,a:8]` | kind + size + align | ⚠️ |

**设计决策：不编码指向类型（pointee type）**

`int*` 和 `double*` 都是 `ptr[s:8,a:8]`。这意味着：
```cpp
struct A { int* p; };
struct B { double* p; };
// layout_signatures_match<A, B>() → true
```

**合理性分析：✅ 合理**

理由：
1. **布局层面**：所有指针在同一平台上大小和对齐相同，`A` 和 `B` 确实可以 `memcpy` 互换
2. **语义层面**：指向类型的差异是语义问题，不是布局问题
3. **TypeLayout 职责**：验证布局，不验证语义——`ptr[s:8,a:8]` 精确描述了指针在内存中的表现
4. **如果编码 pointee type**：签名会变得极其复杂（递归展开指向类型），且对布局无影响

**不完备项**：`int*` 和 `double*` 的 Layout 签名相同 → 但布局确实相同 → **不是漏报，是正确行为**。

#### 7.2.3 结构体/类（非多态）

```
record[s:SIZE,a:ALIGN]{@OFF1:TYPE1,@OFF2:TYPE2,...}
```

**编码的信息**：
- ✅ `sizeof(T)` — 包含尾部 padding
- ✅ `alignof(T)` — 整体对齐要求
- ✅ 每个字段的偏移 (`@OFF`)
- ✅ 每个字段的类型签名（递归）
- ✅ 字段间的 padding（隐含在偏移间隙中）

**完备性论证**：

假设 `⟦A⟧_L = ⟦B⟧_L`，则：
- `sizeof(A) = sizeof(B)` ← from `s:SIZE`
- `alignof(A) = alignof(B)` ← from `a:ALIGN`
- 字段数量相同 ← 逗号分隔的字段列表长度相同
- 每个字段的偏移和类型递归相同 ← `@OFF:TYPE` 逐项匹配

因此 `L_P(A) = L_P(B)` ⟹ `A ≅_mem B`。

**完备性判定：✅ 完备**

#### 7.2.4 继承展平

Layout 层将继承展平为叶子字段序列：

```cpp
struct Base { int x; };
struct Derived : Base { int y; };
// Layout: record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}
// 与 struct Flat { int x; int y; } 完全相同
```

**设计决策：展平继承是故意的**

- 展平后只保留偏移和类型 → 精确反映字节布局
- 不同继承结构可能产生相同的字节布局 → Layout 签名正确相同
- 这是 Layout 层的核心设计目标："只看字节"

**潜在问题：虚基类偏移**

```cpp
struct V { int x; };
struct A : virtual V { int a; };
struct B : virtual V { int b; };
struct D : A, B { int d; };
```

虚基类的偏移由编译器决定，通过 vbase offset table 间接寻址。
Layout 层通过 `offset_of(base_info).bytes` 获取编译器报告的实际偏移，
然后展平。这是正确的——编译器知道虚基类的实际位置。

**完备性判定：✅ 完备**（在展平语义下）

#### 7.2.5 多态类型

```
record[s:SIZE,a:ALIGN,vptr]{@OFF1:TYPE1,...}
```

**编码的信息**：
- ✅ `is_polymorphic` → `,vptr` 标记
- ✅ vptr 占据的空间隐含在 `sizeof` 中
- ❌ vptr 的具体偏移（通常是 0，但标准未定义）
- ❌ vtable 的内容/布局

**完备性分析**：

两个多态类型如果字段偏移和大小都相同，Layout 签名就相同。
vptr 本身的偏移不编码，但因为：
1. vptr 影响 `sizeof`，已编码
2. vptr 影响字段偏移，已编码（通过 `@OFF`）
3. 有 vptr 和没有 vptr 的类型通过 `,vptr` 标记区分

**边界情况**：两个多态类型，虚函数表内容不同但布局相同
→ Layout 签名相同 → **正确**——vtable 内容不影响内存布局。

**完备性判定：✅ 完备**

#### 7.2.6 Union

```
union[s:SIZE,a:ALIGN]{@0:TYPE1,@0:TYPE2,...}
```

**设计决策：Union 成员不展平**

合理性：Union 的多个成员共享同一内存区域（偏移都是 0）。
如果展平嵌套 struct 成员，不同成员的字段会在偏移 0 处碰撞，造成签名混乱。

**完备性分析**：

Union 签名编码了：
- ✅ 总大小和对齐
- ✅ 每个成员的类型签名（不展平）
- ✅ 所有成员都在偏移 0

**边界情况**：Union 成员顺序
```cpp
union U1 { int a; double b; };
union U2 { double b; int a; };
```
P2996 的 `nonstatic_data_members_of` 按声明顺序返回，所以 U1 ≠ U2。
这是**保守安全**的——声明顺序不影响布局，但签名不同。属于"误报"方向。

**完备性判定：✅ 完备**（甚至过度严格）

#### 7.2.7 Enum

```
enum[s:SIZE,a:ALIGN]<UNDERLYING_TYPE>
```

**编码的信息**：
- ✅ sizeof / alignof
- ✅ 底层类型签名
- ❌ 枚举值列表（Layout 层不编码）

**完备性分析**：

同底层类型的不同枚举在 Layout 层签名相同。这是正确的——
`enum Color : uint32_t` 和 `enum Priority : uint32_t` 的字节布局确实相同。

**完备性判定：✅ 完备**

#### 7.2.8 数组

| 类型 | 签名格式 |
|------|---------|
| `char[N]` | `bytes[s:N,a:1]` |
| `T[N]` | `array[s:S,a:A]<ELEM_SIG,N>` |

**设计决策：字节数组归一化**

`char[N]`、`uint8_t[N]`、`std::byte[N]` 都归一化为 `bytes[s:N,a:1]`。
这是合理的——它们的字节布局完全相同。

**设计决策：数组不展开为离散字段**

`int32_t[3]` 不展开为 `@0:i32,@4:i32,@8:i32`。
这意味着：
```cpp
struct A { int32_t arr[3]; };
struct B { int32_t x, y, z; };
// layout_signatures_match<A, B>() → false
```

**合理性**：保守安全。虽然 A 和 B 字节布局相同，但数组和离散字段在 C++ 语义上不同
（数组可以被指针遍历，离散字段不行）。这是故意的保守行为，在 Known Design Limits 中有记录。

**完备性判定：⚠️ 不完备（保守方向）**

这是一个**已知的、故意的**不完备：`int[3]` 和 `int,int,int` 字节布局相同但签名不同。
方向是保守的（不会导致用户错误地认为两个不同的类型是兼容的）。

#### 7.2.9 位域

```
@BYTE.BIT:bits<WIDTH,BASE_TYPE>
```

**编码的信息**：
- ✅ 字节偏移 + 位偏移
- ✅ 位宽
- ✅ 底层类型
- ❌ 位域的分配方向（高位/低位优先）

**已知限制**：位域布局是实现定义的（implementation-defined）。
同一源代码在不同编译器上可能产生不同的位域布局。
TypeLayout 使用编译器的 `offset_of` 和 `bit_size_of`，所以在**同一编译器**内是完备的。

**完备性判定：✅ 同编译器内完备，⚠️ 跨编译器不保证**

#### 7.2.10 CV 限定符处理

```cpp
template <typename T, SignatureMode Mode>
struct TypeSignature<const T, Mode> {
    static consteval auto calculate() noexcept { return TypeSignature<T, Mode>::calculate(); }
};
```

**设计决策：剥离 CV 限定符**

`const int` 和 `int` 的签名相同。合理性：
- `const` 和 `volatile` 不影响内存布局
- `sizeof(const int) == sizeof(int)`, `alignof(const int) == alignof(int)`

**完备性判定：✅ 合理且完备**

---

### 7.3 Definition 层逐类型审计

Definition 层在 Layout 层基础上额外编码：

#### 7.3.1 字段名编码

```
@OFF[field_name]:TYPE  (vs Layout 的 @OFF:TYPE)
```

- ✅ 使用 `identifier_of(member)` 获取字段名
- ✅ 匿名成员使用 `<anon:INDEX>` 标记
- ✅ `has_identifier` 检查确保安全

**完备性**：字段名不同的类型签名必然不同 → ✅ 完备

#### 7.3.2 继承结构保留

```
~base<QualifiedName>:CONTENT   (非虚继承)
~vbase<QualifiedName>:CONTENT  (虚继承)
```

- ✅ 区分虚/非虚继承
- ✅ 基类使用限定名（`qualified_name_for<>`）
- ✅ 基类内容递归生成

**完备性**：不同继承结构的类型签名必然不同 → ✅ 完备

#### 7.3.3 枚举限定名

```
enum<Namespace::EnumName>[s:N,a:M]<underlying>
```

- ✅ 包含枚举类型的限定名
- ❌ 不包含枚举值列表

**完备性**：同名不同命名空间的枚举能区分 → ✅ 完备（对名称）
枚举值变化但名称不变检测不到 → ⚠️ 不完备（但枚举值不影响布局）

#### 7.3.4 嵌套结构保留

Definition 层不展平嵌套 struct：

```
record{@0[x]:record{@0[a]:i32,@4[b]:i32}}  // 保留嵌套
vs Layout:
record{@0:i32,@4:i32}                       // 展平
```

**完备性**：嵌套 vs 展平的结构能区分 → ✅ 完备

---

### 7.4 已知不完备项汇总

| # | 不完备项 | 影响层 | 方向 | 合理性评估 |
|---|---------|:------:|:----:|:--------:|
| I1 | 数组 vs 离散字段 (`int[3]` vs `int,int,int`) | Layout | 保守 | ✅ 故意设计，README 有记录 |
| I2 | Union 成员声明顺序 | 两层 | 保守 | ✅ 保守安全 |
| I3 | 指针指向类型 (`int*` vs `double*`) | Layout | 精确 | ✅ 布局确实相同 |
| I4 | 枚举值列表 | 两层 | — | ✅ 枚举值不影响布局 |
| I5 | 成员函数 / 虚函数表内容 | 两层 | — | ✅ 超出数据布局范围 |
| I6 | 静态成员 | 两层 | — | ✅ 不占实例空间 |
| I7 | access specifier (public/private) | 两层 | — | ⚠️ 见下方分析 |
| I8 | `[[no_unique_address]]` 属性 | 两层 | 精确 | ✅ 偏移由编译器报告 |
| I9 | 跨编译器位域差异 | Layout | — | ⚠️ 实现定义行为 |
| I10 | 类型自身名称 | 两层 | — | ✅ 结构分析设计选择 |

#### I7: access specifier 详细分析

```cpp
struct A { public: int x; private: int y; };
struct B { public: int x; public: int y; };
```

- `access_context::unchecked()` 忽略访问控制
- A 和 B 签名相同

**合理性**：
- 访问控制不影响内存布局 → Layout 层正确
- 访问控制不影响 ABI（大多数编译器） → 合理
- C++ 标准允许编译器对不同 access section 的成员重排序 → **理论上**可能影响布局
- 但实际上主流编译器（GCC、Clang、MSVC）都不重排序

**判定**：⚠️ 理论上不完备，但实际影响为零。如果编译器真的重排序了，
`offset_of` 会报告不同的偏移，签名自然不同。所以最终是完备的。

#### I9: 跨编译器位域差异详细分析

位域的分配规则是实现定义的：
- 分配方向（MSB-first vs LSB-first）
- 跨存储单元边界行为
- 对齐行为

TypeLayout 使用编译器提供的 `offset_of` 和 `bit_size_of`，所以：
- **同编译器**：签名精确反映实际布局 → ✅ 完备
- **跨编译器**：同一源代码可能产生不同签名 → 这是正确行为（布局确实可能不同）

**判定**：✅ 实际上是完备的——签名忠实反映编译器报告的布局。

---

### 7.5 签名语法无歧义性

**问题**：给定一个签名字符串，能否唯一解析出它编码的信息？

签名使用嵌套的括号结构：
```
record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}
```

**语法结构**：
```
signature ::= arch_prefix type_sig
arch_prefix ::= '[' digits '-' endian ']'
type_sig ::= primitive_sig | record_sig | union_sig | enum_sig | array_sig | bitfield_sig
primitive_sig ::= KIND '[s:' digits ',a:' digits ']'
record_sig ::= 'record[s:' digits ',a:' digits [',vptr' | ',polymorphic'] ']{' field_list '}'
field_list ::= field (',' field)*
field ::= '@' offset ':' type_sig                    (Layout)
       | '@' offset '[' name ']:' type_sig           (Definition)
```

**无歧义性论证**：

1. **前缀无歧义**：`record` / `union` / `enum` / `array` / `bits` / `bytes` / 基本类型前缀都不同
2. **定界符无歧义**：`{...}` 用于字段列表，`<...>` 用于模板参数，`[...]` 用于属性
3. **嵌套可解析**：括号嵌套是正则匹配的，不存在歧义
4. **字段分隔**：`,` 在字段列表内部分隔，与 `[s:N,a:M]` 中的 `,` 不冲突
   （因为 `[...]` 括号界定了属性区域）

**潜在歧义点**：字段名中包含特殊字符？
- C++ 标识符只能包含 `[a-zA-Z0-9_]`，不会包含 `]`, `:`, `,` 等分隔符
- `<anon:N>` 中的 `<>` 不会与 `enum<>` 或 `array<>` 冲突（出现在 `[...]` 内部）

**判定：✅ 签名语法无歧义**

---

### 7.6 综合评估

#### Layout 层完备性总评

| 维度 | 评估 | 说明 |
|------|:----:|------|
| 基本类型 | ✅ | kind + size + align 三元组完全区分 |
| 结构体 | ✅ | sizeof + alignof + 递归字段完全区分 |
| 继承 | ✅ | 展平后字节布局完全编码 |
| 多态 | ✅ | vptr 标记 + sizeof 影响 |
| Union | ✅ | 不展平，成员类型完全编码 |
| Enum | ✅ | 底层类型完全编码 |
| 数组 | ⚠️ | `int[3]` vs `int,int,int` 保守不完备 |
| 位域 | ✅ | 同编译器内完备 |
| 指针 | ✅ | 同大小指针签名相同（正确行为） |
| CV 限定 | ✅ | 剥离不影响布局的限定符 |

**总评：4.8/5 — 高度完备**

唯一的不完备项（数组 vs 离散字段）是故意的保守设计，方向安全。

#### Definition 层完备性总评

| 维度 | 评估 | 说明 |
|------|:----:|------|
| Layout 完备性 | ✅ | 继承 Layout 的全部信息（V3 投影） |
| 字段名 | ✅ | identifier_of 精确编码 |
| 继承结构 | ✅ | base/vbase + 限定名 |
| 枚举名称 | ✅ | qualified_name 编码 |
| 嵌套结构 | ✅ | 树结构保留 |
| 匿名成员 | ✅ | `<anon:N>` 标记 |

**总评：4.9/5 — 近乎完美**

Definition 层在 Layout 层基础上严格增加了信息，没有引入新的不完备性。

#### 发现的问题与建议

**无关键缺陷。** 签名系统的设计在以下三个维度表现良好：

1. **Soundness（无误报）**：由 PROOFS.md Theorem 3.2 保证 — 签名匹配一定意味着布局兼容
2. **Completeness（无漏报）**：本分析确认除 `数组vs离散字段` 外无漏报，且该项为故意设计
3. **Unambiguity（无歧义）**：签名语法可唯一解析

**建议**：
- 在 PROOFS.md 中补充一个 §8 "Completeness Audit" 小节，引用本分析的结论
- 在 Known Design Limits 中明确标注"数组不展开"是**唯一**的布局层保守不完备项
