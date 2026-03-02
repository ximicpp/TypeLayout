# Boost.TypeLayout 设计修改：基类子对象签名策略

## 一、修改概述

将基类子对象的布局签名策略从 **方案 C（嵌套子对象）** 改为 **方案 B（展开基类成员到顶层）**。

**修改动机**：当前方案将继承结构信息混入布局签名层，违反了两层签名架构的职责分离原则，导致物理布局相同的类型被错误判定为布局不兼容。

---

## 二、背景：两层签名架构

TypeLayout 的签名分为两层，各有明确职责，**严禁混淆**：

| 层级 | 名称 | 职责 | 编码内容 |
|------|------|------|----------|
| 第一层 | **类型签名 (Type Signature)** | 标识类型的结构身份 | 类型名/结构描述、成员名与类型、继承关系、访问控制等语义信息 |
| 第二层 | **布局签名 (Layout Signature)** | 标识类型的物理内存布局 | `sizeof`、`alignof`、每个数据字段的 `{offset, type_layout}` |

**判定规则**：

- 类型签名相同 → 结构等价（同一种类型结构）
- 布局签名相同 → 物理布局二进制兼容

两者独立：不同类型可以有相同布局签名（布局兼容），同一类型在不同平台可以有不同布局签名。

---

## 三、设计原则（贯穿所有决策）

1. **信任可观测量** — `sizeof(T)` + `alignof(T)` + 每个数据成员的 `{offset_of, type}` 已完整编码物理布局。不重复建模可由这三者推导的信息。
2. **布局签名只编码物理布局** — 继承关系、成员名、访问控制等语义信息属于类型签名层，不得混入布局签名层。
3. **反射给什么就记什么** — 不猜测、不推断。P2996 静态反射返回的 `offset_of` / `sizeof` / `alignof` 是唯一数据来源。

---

## 四、当前方案（方案 C：嵌套子对象）— 需要被替换

### 做法

基类作为一个完整的嵌套 `layout` 子对象嵌入布局签名，保留基类边界。

### 签名示例

```cpp
struct Base { int b; };
struct A : Base { int x; };
```

当前方案生成的布局签名：

```
layout[s:8,a:4]{ base@0:layout[s:4,a:4]{ @0:i32 }, @4:i32 }
```

### 当前方案存在的问题

**问题 1：布局相同的类型被判为不兼容**

```cpp
struct Base { int b; };
struct A : Base { int x; };   // 物理布局: [b(4)][x(4)] sizeof=8
struct C { int b; int x; };   // 物理布局: [b(4)][x(4)] sizeof=8
```

A 和 C 物理布局完全一致，但布局签名不同：

```
A: layout[s:8,a:4]{ base@0:layout[s:4,a:4]{ @0:i32 }, @4:i32 }
C: layout[s:8,a:4]{ @0:i32, @4:i32 }
```

结果：布局签名不同。**错误** — 违反原则 2。

**问题 2：EBO 场景下产生幽灵节点**

```cpp
struct Empty {};
struct A : Empty { int x; };  // sizeof=4, x at offset 0
struct B { int x; };           // sizeof=4, x at offset 0
```

A 和 B 物理布局完全一致，但签名不同：

```
A: layout[s:4,a:4]{ base@0:layout[s:1,a:1]{}, @0:i32 }
B: layout[s:4,a:4]{ @0:i32 }
```

A 多了一个不占空间的空基类幽灵节点。**错误**。

**问题 3：多重继承下 offset 语义模糊**

```cpp
struct B1 { int a; };
struct B2 { double b; };
struct D : B1, B2 { int c; };
// 典型布局: [a(4)][pad(4)][b(8)][c(4)][pad(4)] sizeof=24
```

嵌套方案中，base 内部的 `@0` 是相对基类子对象起始的偏移还是绝对偏移？需要额外约定，增加复杂度和歧义。

---

## 五、修改方案（方案 B：展开基类成员到顶层）

### 做法

递归遍历所有基类，收集其数据成员，将所有数据成员按 P2996 反射返回的**绝对 offset** 平铺到顶层布局签名中。不在布局签名中保留任何基类边界信息。

### 签名示例

```cpp
struct Base { int b; };
struct A : Base { int x; };
```

修改方案生成的布局签名：

```
layout[s:8,a:4]{ @0:i32, @4:i32 }
```

### 各场景验证

**场景 1：空基类（EBO 生效）**

```cpp
struct Empty {};
struct A : Empty { int x; };  // sizeof=4, x at offset 0
struct B { int x; };           // sizeof=4, x at offset 0
```

```
A: layout[s:4,a:4]{ @0:i32 }
B: layout[s:4,a:4]{ @0:i32 }
```

布局签名相同。✅ 正确 — 物理布局确实相同。

**场景 2：非空基类**

```cpp
struct Base { int b; };
struct A : Base { int x; };   // sizeof=8
struct C { int b; int x; };   // sizeof=8
```

```
A: layout[s:8,a:4]{ @0:i32, @4:i32 }
C: layout[s:8,a:4]{ @0:i32, @4:i32 }
```

布局签名相同。✅ 正确 — 物理布局确实相同。类型签名不同（A 有继承关系，C 没有），结构差异在类型签名层正确保留。

**场景 3：多重继承**

```cpp
struct B1 { int a; };
struct B2 { double b; };
struct D : B1, B2 { int c; };
// 布局: [a(4)][pad(4)][b(8)][c(4)][pad(4)] sizeof=24
```

```
D: layout[s:24,a:8]{ @0:i32, @8:f64, @16:i32 }
```

所有 offset 均为绝对偏移，无歧义。✅

**场景 4：菱形继承（无虚继承）**

```cpp
struct G { int g; };
struct P1 : G { int p1; };
struct P2 : G { int p2; };
struct D : P1, P2 { int d; };
// 布局: [G::g(4)][p1(4)][G::g(4)][p2(4)][d(4)] sizeof=20
```

```
D: layout[s:20,a:4]{ @0:i32, @4:i32, @8:i32, @12:i32, @16:i32 }
```

两份 `G::g` 的副本各自在不同 offset，如实记录。✅

**场景 5：虚继承**

```cpp
struct V { int v; };
struct A : virtual V { int a; };
// 布局因 ABI 而异，假设 v at offset 12, a at offset 8, sizeof=16
```

```
A: layout[s:16,a:8]{ @8:i32, @12:i32 }
```

vptr/vbptr 占用的空间体现在 sizeof 和成员 offset 的间隙中，不显式编码。✅ 符合原则 1。

---

## 六、修改对比总结

| 维度 | 当前方案（嵌套） | 修改方案（展开） |
|------|------------------|------------------|
| 签名结构 | 树形（有嵌套层级） | 扁平（只有一层） |
| 基类边界 | 保留 | 不保留 |
| offset 语义 | 模糊（相对/绝对需额外约定） | 统一绝对偏移 |
| EBO 空基类 | 产生幽灵节点 | 无痕迹 |
| 布局等价判定 | 可能误判不兼容 | 正确判定兼容 |
| 继承信息 | 泄漏到布局签名层 | 只在类型签名层保留 |
| 实现复杂度 | 需要嵌套构造和递归比较 | 平铺收集，线性比较 |
| 原则符合性 | 违反原则 2 | 完全符合 |

---

## 七、实现要点

### 成员收集算法

```
function collect_layout_fields(T):
    fields = []
    for each base B of T (按声明顺序):
        fields += collect_layout_fields(B)       // 递归收集基类成员
    for each direct data member m of T:
        fields += { offset: offset_of(m), layout: layout_signature(type_of(m)) }
    return fields
```

### 注意事项

1. **所有 offset 必须是相对于最终派生类 T 起始地址的绝对偏移**，而非相对于基类子对象的偏移。P2996 的 `offset_of` 在顶层类型上下文中使用时应直接返回绝对偏移。
2. **空基类不会贡献任何成员**，自然消失，无需特判。
3. **成员按 offset 升序排列**。若两个成员 offset 相同（如 `[[no_unique_address]]` 场景），则如实并列记录。
4. **继承关系信息（哪些成员来自哪个基类）只在类型签名层保留**，布局签名层完全不关心。

---

## 八、与 vptr 删除修改的关系

本修改与"删除 `introduces_vptr` 启发式"修改相互独立但理念一致：

| 修改 | 删除的内容 | 共同原则 |
|------|-----------|----------|
| 删除 vptr | 删除布局签名中的 `@0:vptr[s:8,a:8]` | 原则 1：sizeof + offset 已隐式编码 |
| 展开基类 | 删除布局签名中的 `base@N:layout{...}` 嵌套 | 原则 2：继承关系不属于物理布局 |

两者合并后，布局签名变为纯粹的 `layout[s:S,a:A]{ @offset:type, ... }` 扁平列表，只编码物理可观测量。