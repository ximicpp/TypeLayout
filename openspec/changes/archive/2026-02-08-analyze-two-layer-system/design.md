# 两层签名系统：功能、原理与实例分析

## 1. 核心问题

C++ 中有一个长期存在的问题：**如何在编译期判断两个类型在内存中是否兼容？**

```cpp
// Process A 定义
struct SensorData { int32_t id; double value; };

// Process B 定义
struct SensorData { int32_t id; double value; };
```

两个进程各自编译，各自定义了 `SensorData`。它们能通过共享内存安全通信吗？
C++ 标准没有提供回答这个问题的工具。TypeLayout 回答了它。

---

## 2. 为什么需要两层

一层不够吗？考虑这个例子：

```cpp
// --- 场景 A：字段名不同但布局相同 ---
struct Point  { int32_t x; double y; };
struct Coord  { int32_t a; double b; };
```

从**内存布局**角度看，`Point` 和 `Coord` 完全相同：
- 都是 16 字节
- 都在偏移 0 处有一个 4 字节整数
- 都在偏移 8 处有一个 8 字节浮点数

如果你要做 `memcpy` 或共享内存，它们是兼容的。

```cpp
// --- 场景 B：布局相同但语义不同 ---
struct Velocity  { int32_t vx; double vy; };
struct Position  { int32_t px; double py; };
```

布局也相同。但如果一个 API 期望 `Position`，你传了 `Velocity`，这是逻辑错误。

**这就是两层的意义**：

| 层 | 回答的问题 | 匹配条件 |
|----|-----------|----------|
| **Layout**（第一层）| 这两个类型在内存中字节兼容吗？ | 相同的字节排列 |
| **Definition**（第二层）| 这两个类型在结构上等价吗？ | 相同的字段名、类型、层次 |

---

## 3. Layout 签名：纯字节身份

### 3.1 核心原理

Layout 签名回答：**"如果我把类型 T 的内存直接 memcpy 到类型 U 的内存，数据是否完整保留？"**

它通过三个操作实现：

1. **继承展平**：把基类字段"拉平"到派生类的绝对偏移位置
2. **组合展平**：把嵌套 struct 的字段递归展开为叶字段
3. **名字擦除**：不记录字段名，只记录类型和偏移

### 3.2 实例：继承展平

```cpp
struct Base { int32_t id; };
struct Derived : Base { double value; };
struct Flat { int32_t id; double value; };
```

**Layout 签名**：
```
Derived: [64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}
Flat:    [64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}
                                  ^-- 完全相同！
```

`Derived` 的继承被展平了。`Base::id` 出现在绝对偏移 `@0`，不再有任何 "基类" 的概念。

**验证**（编译期）：
```cpp
static_assert(layout_signatures_match<Derived, Flat>());  // true!
```

### 3.3 实例：组合展平

```cpp
struct Inner { int32_t a; int32_t b; };
struct Composed { Inner x; };
struct Flat { int32_t a; int32_t b; };
```

**Layout 签名**：
```
Composed: [64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}
Flat:     [64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}
```

`Composed` 中的 `Inner x` 被递归展开为两个独立的 `i32` 叶字段。

**深层嵌套同样有效**：
```cpp
struct Deep { int32_t p; int32_t q; };
struct Mid { Deep d; int32_t r; };
struct Outer { Mid m; };
struct DeepFlat { int32_t p; int32_t q; int32_t r; };

static_assert(layout_signatures_match<Outer, DeepFlat>());  // true!
```

### 3.4 实例：多态类型保护

```cpp
struct Poly { virtual void foo(); int32_t x; };
struct NonPoly { int32_t x; };
```

**Layout 签名**：
```
Poly:    [64-le]record[s:16,a:8,vptr]{@8:i32[s:4,a:4]}
NonPoly: [64-le]record[s:4,a:4]{@0:i32[s:4,a:4]}
```

注意 `Poly` 有 `,vptr` 标记，且 `x` 的偏移从 `@0` 变为 `@8`（因为 vptr 占据了前 8 字节）。
两者**不匹配** -- 防止了把非多态类型误当作多态类型。

### 3.5 实例：字节数组归一化

```cpp
struct BufChar { char buf[32]; };
struct BufU8   { uint8_t buf[32]; };
struct BufByte { std::byte buf[32]; };

static_assert(layout_signatures_match<BufChar, BufU8>());    // true
static_assert(layout_signatures_match<BufChar, BufByte>());  // true
```

`char[32]`、`uint8_t[32]`、`std::byte[32]` 在内存中完全相同，都归一化为 `bytes[s:32,a:1]`。

---

## 4. Definition 签名：结构身份

### 4.1 核心原理

Definition 签名回答：**"这两个类型的结构定义是否等价？字段名、类型、继承层次是否完全一致？"**

它与 Layout 的关键差异：

| 维度 | Layout | Definition |
|------|--------|------------|
| 字段名 | ❌ 不记录 | ✅ 记录 `[name]` |
| 继承 | 展平为绝对偏移 | 保留 `~base<Name>` 树结构 |
| 嵌套 struct | 展平 | 保留 `record{...}` 树结构 |
| 枚举名 | ❌ 只有底层类型 | ✅ 包含限定名 |
| 多态标记 | `vptr` | `polymorphic` |

### 4.2 实例：字段名区分

```cpp
struct Simple  { int32_t x; double y; };
struct Simple2 { int32_t a; double b; };
```

**Layout 签名**（相同）：
```
Simple:  [64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}
Simple2: [64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}
```

**Definition 签名**（不同）：
```
Simple:  [64-le]record[s:16,a:8]{@0[x]:i32[s:4,a:4],@8[y]:f64[s:8,a:8]}
Simple2: [64-le]record[s:16,a:8]{@0[a]:i32[s:4,a:4],@8[b]:f64[s:8,a:8]}
                                     ^^^                   ^^^
```

```cpp
static_assert( layout_signatures_match<Simple, Simple2>());      // true  -- 布局兼容
static_assert(!definition_signatures_match<Simple, Simple2>());   // false -- 结构不等价
```

### 4.3 实例：继承树保留

```cpp
struct Base { int32_t id; };
struct Derived : Base { double value; };
struct Flat { int32_t id; double value; };
```

**Definition 签名**：
```
Derived: [64-le]record[s:16,a:8]{~base<test_inheritance::Base>:record[s:4,a:4]{@0[id]:i32[s:4,a:4]},@8[value]:f64[s:8,a:8]}
Flat:    [64-le]record[s:16,a:8]{@0[id]:i32[s:4,a:4],@8[value]:f64[s:8,a:8]}
```

`Derived` 的签名保留了 `~base<test_inheritance::Base>` 前缀，清楚表达了继承关系。
`Flat` 没有基类前缀。两者 **Definition 不匹配**，但 **Layout 匹配**。

### 4.4 实例：枚举限定名

```cpp
enum class Color : uint8_t { Red, Green, Blue };
enum class Shape : uint8_t { Circle, Square, Triangle };
```

**Layout 签名**（相同 -- 都是 1 字节 unsigned）：
```
Color: [64-le]enum[s:1,a:1]<u8[s:1,a:1]>
Shape: [64-le]enum[s:1,a:1]<u8[s:1,a:1]>
```

**Definition 签名**（不同 -- 包含限定名）：
```
Color: [64-le]enum<test_enum_identity::Color>[s:1,a:1]<u8[s:1,a:1]>
Shape: [64-le]enum<test_enum_identity::Shape>[s:1,a:1]<u8[s:1,a:1]>
```

```cpp
static_assert( layout_signatures_match<Color, Shape>());      // true  -- 底层类型相同
static_assert(!definition_signatures_match<Color, Shape>());   // false -- 不同的枚举
```

### 4.5 实例：命名空间防碰撞

```cpp
namespace ns1 { struct Tag { int32_t id; }; }
namespace ns2 { struct Tag { int32_t id; }; }

struct A : ns1::Tag { double v; };
struct B : ns2::Tag { double v; };
```

**Definition 签名**：
```
A: ...{~base<test_base_ns1::Tag>:record{...},@8[v]:f64...}
B: ...{~base<test_base_ns2::Tag>:record{...},@8[v]:f64...}
         ^^^^^^^^^^^^^^^^           ^^^^^^^^^^^^^^^^
```

基类的完全限定名 (`ns1::Tag` vs `ns2::Tag`) 防止了来自不同命名空间的同名类型的误匹配。

### 4.6 实例：虚继承标记

```cpp
struct VBase { int32_t x; };
struct Derived : virtual VBase { int32_t y; };
```

**Definition 签名**：
```
[64-le]record[s:16,a:8]{~vbase<test_virtual_inherit::VBase>:record[s:4,a:4]{@0[x]:i32[s:4,a:4]},@8[y]:i32[s:4,a:4]}
                         ^^^^^^
```

注意 `~vbase<>` 而非 `~base<>`，明确标识了虚继承关系。

---

## 5. 投影关系

两层之间存在严格的数学投影关系：

> **Definition 匹配 ⟹ Layout 匹配**（反之不成立）

这意味着 Definition 是 Layout 的**严格细化**。

```
Definition 等价集合
    ⊂
Layout 等价集合
    ⊂
所有类型集合
```

### 5.1 直觉理解

Definition 签名编码了 Layout 签名的所有信息（size、alignment、偏移、类型），还额外编码了字段名、继承关系、限定名。如果两个类型在更严格的 Definition 层面都完全相同，它们在更宽松的 Layout 层面当然也相同。

### 5.2 实例验证

```cpp
// Definition 匹配的一对
struct Outer1 { Inner inner; double d; };
struct Outer2 { Inner inner; double d; };
// 字段名相同、类型相同、结构相同 → Definition 匹配 → Layout 也匹配

static_assert( definition_signatures_match<Outer1, Outer2>());
static_assert( layout_signatures_match<Outer1, Outer2>());   // 投影保证

// Layout 匹配但 Definition 不匹配的一对
struct Derived : Base { double value; };
struct Flat { int32_t id; double value; };
// 内存布局相同，但一个有继承一个没有

static_assert( layout_signatures_match<Derived, Flat>());     // 布局兼容
static_assert(!definition_signatures_match<Derived, Flat>());  // 结构不同
```

---

## 6. 签名格式解析

### 6.1 完整格式语法

```
signature     := arch_prefix type_sig
arch_prefix   := "[" bits "-" endian "]"       -- 例: [64-le]
bits          := "32" | "64"
endian        := "le" | "be"

type_sig      := record_sig | enum_sig | union_sig | array_sig | fundamental_sig
record_sig    := "record[s:" SIZE ",a:" ALIGN poly "]{" content "}"
poly          := "" | ",vptr" | ",polymorphic"
content       := field ("," field)*
field         := "@" OFFSET name ":" type_sig      -- 普通字段
               | "@" BYTE "." BIT name ":bits<" WIDTH "," type_sig ">"  -- 位域

name          := ""                                -- Layout 模式
               | "[" IDENTIFIER "]"                -- Definition 模式
               | "[<anon:" INDEX ">]"              -- 匿名成员

enum_sig      := "enum" qname "[s:" SIZE ",a:" ALIGN "]<" type_sig ">"
qname         := ""                                -- Layout 模式
               | "<" QUALIFIED_NAME ">"            -- Definition 模式

union_sig     := "union[s:" SIZE ",a:" ALIGN "]{" field ("," field)* "}"
array_sig     := "array[s:" SIZE ",a:" ALIGN "]<" type_sig "," COUNT ">"
               | "bytes[s:" SIZE ",a:1]"           -- 字节数组归一化

fundamental_sig := NAME "[s:" SIZE ",a:" ALIGN "]"
NAME          := "i8" | "u8" | "i16" | ... | "f32" | "f64" | "f80"
               | "char" | "wchar" | "bool" | "ptr" | "ref" | "fnptr" | ...
```

### 6.2 一个完整签名的解读

```
[64-le]record[s:16,a:8]{~base<test_inheritance::Base>:record[s:4,a:4]{@0[id]:i32[s:4,a:4]},@8[value]:f64[s:8,a:8]}
```

逐段解读：
- `[64-le]` -- 64 位小端平台
- `record[s:16,a:8]` -- 这是一个 struct，总大小 16 字节，对齐 8 字节
- `{...}` -- 内容
  - `~base<test_inheritance::Base>` -- 继承自 `test_inheritance::Base`
  - `:record[s:4,a:4]{@0[id]:i32[s:4,a:4]}` -- 基类是一个 4 字节 struct，偏移 0 处有一个名为 `id` 的 `int32_t`
  - `,@8[value]:f64[s:8,a:8]` -- 偏移 8 处有一个名为 `value` 的 `double`

---

## 7. 实际应用场景

### 7.1 共享内存 / IPC -- 使用 Layout

两个独立编译的进程通过共享内存通信：

```cpp
// process_a.cpp
struct SensorReading { int32_t id; double temp; double humidity; };

// process_b.cpp
struct SensorReading { int32_t id; double temp; double humidity; };

// 验证：两端的类型在内存中字节兼容
constexpr auto sig = get_layout_signature<SensorReading>();
// 将签名嵌入共享内存头部，接收端校验
```

**为什么用 Layout 而不是 Definition？** 因为共享内存只关心字节排列。即使两边字段名不同（一个叫 `temp`，一个叫 `temperature`），只要偏移和类型一致，`memcpy` 就是安全的。

### 7.2 序列化版本检查 -- 使用 Definition

```cpp
// v1.0
struct Config { int32_t timeout; double threshold; };

// v2.0 -- 改了字段名
struct Config { int32_t max_wait; double threshold; };
```

Layout 签名相同（布局没变），但 Definition 签名不同（`timeout` → `max_wait`）。
如果你的序列化格式依赖字段名（如 JSON），这个改动会导致兼容性问题。Definition 层能捕捉到它。

### 7.3 插件接口验证 -- 使用 Definition

```cpp
// host.hpp (宿主程序)
struct PluginData { int32_t version; double score; };

// plugin.cpp (动态加载)
struct PluginData { int32_t version; double score; };

// 宿主验证插件的类型定义与自身一致
static_assert(definition_signatures_match<HostPluginData, PluginPluginData>());
```

插件 ABI 兼容性需要 Definition 级别的检查 -- 不仅布局要一致，字段名和结构也要一致，确保语义匹配。

### 7.4 编译器 ABI 跨平台检查 -- 使用 Layout

```cpp
// 在 Linux x86_64 上编译
constexpr auto linux_sig = get_layout_signature<NetworkPacket>();
// "[64-le]record[s:24,a:8]{@0:u32[s:4,a:4],@8:f64[s:8,a:8],@16:u32[s:4,a:4]}"

// 在 Windows x86_64 上编译
constexpr auto win_sig = get_layout_signature<NetworkPacket>();
// 如果两者相同，说明这个结构在两个平台上的内存布局一致
```

---

## 8. 设计哲学：结构分析 vs 名义分析

TypeLayout 采用**结构分析**（Structural Analysis），而非名义分析（Nominal Analysis）。

这意味着：
- 签名**不包含类型自身的名字** -- `struct Point` 和 `struct Coord` 如果结构完全一致，它们的 Definition 签名就相同
- 签名反映的是 "这个类型的内部长什么样"，而不是 "这个类型叫什么名字"

```cpp
namespace ns1 { struct Point { int32_t x; double y; }; }
namespace ns2 { struct Coord { int32_t x; double y; }; }

// Definition 签名相同！因为字段名、类型、偏移完全一致
static_assert(definition_signatures_match<ns1::Point, ns2::Coord>());
```

这是**有意的设计选择**。TypeLayout 回答的问题是 "两个类型的结构是否等价"，不是 "它们是否是同一个类型"。

---

## 9. 技术实现概览

### 9.1 三文件架构

```
fwd.hpp              -- 基础设施：FixedString, SignatureMode, to_fixed_string
signature_detail.hpp -- 反射引擎：P2996 meta-operations, TypeSignature 特化
signature.hpp        -- 公共 API：get_layout_signature, get_definition_signature
```

### 9.2 核心技术

- **P2996 静态反射**：使用 `std::meta::nonstatic_data_members_of`、`offset_of`、`bases_of` 等编译期反射 API 遍历类型结构
- **consteval 全量计算**：所有签名生成在编译期完成，零运行时开销
- **FixedString 模板**：编译期字符串拼接，支持 `operator+` 和 `operator==`
- **折叠表达式**：用 `(f<Is>() + ...)` 在编译期拼接任意数量字段

### 9.3 Layout 展平算法

```
layout_all_prefixed<T, OffsetAdj>:
  for each base B of T:
    layout_all_prefixed<B, offset_of(B) + OffsetAdj>     -- 递归基类
  for each field F of T:
    if F is class (non-union):
      layout_all_prefixed<F, offset_of(F) + OffsetAdj>   -- 递归展开
    else:
      emit ",@{offset_of(F)+OffsetAdj}:{TypeSig<F>}"     -- 叶字段
```

### 9.4 Definition 保留算法

```
definition_content<T>:
  for each base B of T:
    emit "~base<qualified_name(B)>:{TypeSig<B, Definition>}"  -- 保留基类
  for each field F of T:
    emit "@{offset}[{name}]:{TypeSig<F, Definition>}"          -- 保留字段名和树结构
```

---

## 10. 总结

| 维度 | Layout（第一层） | Definition（第二层） |
|------|-----------------|---------------------|
| 回答 | 字节兼容吗？ | 结构等价吗？ |
| 精度 | 宽松 | 严格 |
| 继承 | 展平 | 保留树 |
| 组合 | 展平 | 保留树 |
| 字段名 | 擦除 | 保留 |
| 枚举名 | 擦除 | 保留 |
| 用途 | IPC、memcpy、ABI | API 兼容、ODR、序列化 |
| 保证 | `sig_match ⟹ memcmp 兼容` | `sig_match ⟹ 结构等价` |
| 投影 | 是 Definition 的宽松映射 | 是 Layout 的严格细化 |
