## 1. C++ 生态调研
- [x] 1.1 RTTI (`typeid`, `type_info`)
- [x] 1.2 Boost.TypeIndex
- [x] 1.3 ABI Name Mangling
- [x] 1.4 `sizeof`/`offsetof`/`static_assert` 手动验证
- [x] 1.5 Boost.PFR (Magic Get)
- [x] 1.6 ABI Compliance Checker (外部工具)

## 2. 跨语言调研
- [x] 2.1 Rust `std::mem::size_of` + `#[repr(C)]`
- [x] 2.2 Java 序列化 `serialVersionUID`
- [x] 2.3 Protocol Buffers / FlatBuffers schema
- [x] 2.4 Cap'n Proto zero-copy

## 3. 对比分析
- [x] 3.1 多维度对比矩阵
- [x] 3.2 TypeLayout 的不可替代区间
- [x] 3.3 Boost 评审者可能的质疑点

## 4. 结论
- [x] 4.1 TypeLayout 的独特价值定位

---

## 5. 调研结果

### 5.1 C++ 生态中的类型签名方案

#### 5.1.1 RTTI (`typeid` / `type_info`)

**机制**：运行时类型识别，通过 `typeid(T).name()` 获取类型名，`typeid(T).hash_code()` 获取哈希。

**能力**：
- ✅ 区分不同**名义类型**（`struct A` vs `struct B`）
- ✅ 运行时可用
- ❌ 不提供布局信息（sizeof、字段偏移、对齐）
- ❌ `name()` 格式是实现定义的，不可移植
- ❌ 两个结构相同但名称不同的类型被视为不同
- ❌ 两个名称相同但结构不同的类型（ODR 违规）无法区分
- ❌ 需要运行时开销（vtable 查找）
- ❌ 可通过 `-fno-rtti` 关闭

**TypeLayout 差异**：
| 维度 | RTTI | TypeLayout |
|------|------|-----------|
| 分析类型 | 名义分析 | 结构分析 |
| 布局信息 | ❌ 无 | ✅ 完整（偏移、大小、对齐） |
| 时机 | 运行时 | 编译时（零开销） |
| 可关闭性 | 是 (`-fno-rtti`) | 否（语言内置反射） |
| 跨 TU 一致性 | 不保证 | ✅ 保证（同结构 → 同签名） |

**结论**：RTTI 回答"这是什么类型？"，TypeLayout 回答"这个类型的内存长什么样？"。
两者解决的是完全不同的问题，没有替代关系。

#### 5.1.2 Boost.TypeIndex

**机制**：对 RTTI 的可移植封装，提供 `type_id<T>()` 和 `type_id_runtime(obj)`。

**能力**：
- ✅ 跨编译器一致的类型名称
- ✅ 可在 `-fno-rtti` 模式下工作（使用编译时模板）
- ❌ 仍然是名义分析，不提供布局信息
- ❌ 不区分结构不同但同名的类型

**TypeLayout 差异**：与 RTTI 相同——Boost.TypeIndex 是 RTTI 的改良版，
但仍然是名义分析，不提供布局信息。

**结论**：Boost.TypeIndex 和 TypeLayout 是互补的，不是竞争关系。

#### 5.1.3 ABI Name Mangling

**机制**：编译器将 C++ 函数/类型名编码为唯一的符号名（如 Itanium ABI 的 `_ZN1A1fEv`）。

**能力**：
- ✅ 精确编码类型名称、命名空间、模板参数
- ✅ 链接器使用，跨 TU 一致
- ❌ 不编码布局信息（偏移、padding、对齐）
- ❌ 格式是 ABI 特定的（Itanium vs MSVC），不可移植
- ❌ 编译器内部使用，无标准 API 访问
- ❌ 不编码字段信息（只编码类型名）

**TypeLayout 差异**：
- Mangled name 编码"类型的名称身份"
- TypeLayout 编码"类型的结构身份（布局 + 字段名 + 继承）"
- 两者编码的信息**完全不重叠**

**结论**：Name mangling 解决的是链接器符号唯一性问题，不是布局验证问题。

#### 5.1.4 `sizeof` / `offsetof` / `static_assert` 手动验证

**机制**：程序员手动编写断言来验证类型布局。

```cpp
static_assert(sizeof(PacketHeader) == 16);
static_assert(offsetof(PacketHeader, magic) == 0);
static_assert(offsetof(PacketHeader, version) == 4);
// ... 每个字段都要手动写
```

**能力**：
- ✅ 编译时验证
- ✅ 精确控制
- ❌ 手动维护，容易遗漏
- ❌ 新增字段时必须手动更新断言
- ❌ 无法自动覆盖所有字段
- ❌ 跨 TU 比较需要硬编码期望值

**TypeLayout 差异**：

| 维度 | 手动 static_assert | TypeLayout |
|------|-------------------|-----------|
| 完备性 | ❌ 手动，易遗漏 | ✅ 自动覆盖所有字段 |
| 维护成本 | O(n) per field | O(1) per type |
| 新增字段 | 需手动添加断言 | 自动检测 |
| 跨类型比较 | 需硬编码值 | `signatures_match<T,U>()` |

**这是 TypeLayout 最直接的替代对象。** TypeLayout 的核心价值就是"自动化的、完备的 static_assert"。

**结论**：TypeLayout 是 `sizeof`/`offsetof`/`static_assert` 的**自动化替代方案**。
这是最重要的价值论据。

#### 5.1.5 Boost.PFR (Magic Get)

**机制**：无需宏或反射即可访问聚合类型的字段（利用结构化绑定技巧）。

**能力**：
- ✅ 编译时获取字段数量和类型
- ✅ 不需要 P2996
- ❌ 仅支持聚合类型（无构造函数、无基类、无虚函数）
- ❌ 不提供字段偏移信息
- ❌ 不提供字段名（C++17/20 中无法获取）
- ❌ 不支持继承、多态、位域

**TypeLayout 差异**：

| 维度 | Boost.PFR | TypeLayout |
|------|-----------|-----------|
| 类型支持范围 | 仅聚合类型 | 所有 class/struct/union/enum |
| 字段偏移 | ❌ | ✅ |
| 字段名 | ❌ | ✅ (Definition 层) |
| 继承支持 | ❌ | ✅ |
| 位域支持 | ❌ | ✅ |
| 编译器要求 | C++17 | C++26 (P2996) |

**结论**：Boost.PFR 是"穷人的反射"，在 P2996 可用之前是最佳选择。
TypeLayout 基于 P2996 提供了**严格超集**的能力。两者面向不同的 C++ 标准时代。

#### 5.1.6 ABI Compliance Checker (外部工具)

**机制**：`abi-compliance-checker` 等工具对比两个库版本的 ABI 兼容性。

**能力**：
- ✅ 检测 ABI 变化（函数签名、vtable、类布局）
- ✅ 全面（包括成员函数、虚函数表）
- ❌ 外部工具，不是 C++ 代码的一部分
- ❌ 运行时/构建时检查，不是编译时
- ❌ 需要两个完整的编译产物进行对比
- ❌ 输出是报告，不是可编程的布尔值

**TypeLayout 差异**：

| 维度 | ABI Checker | TypeLayout |
|------|-------------|-----------|
| 集成方式 | 外部工具 | 语言内库 |
| 检查时机 | 构建后 | 编译时 |
| 可编程性 | ❌ 报告输出 | ✅ `consteval bool` |
| 覆盖范围 | ABI 全面（含函数） | 数据布局专精 |
| 部署要求 | 需安装工具 | header-only |

**结论**：ABI Checker 更全面但更重。TypeLayout 更轻量、更集成、更早期（编译时）。
两者互补——TypeLayout 处理数据布局，ABI Checker 处理函数签名和 vtable。

---

### 5.2 跨语言调研

#### 5.2.1 Rust `std::mem` + `#[repr(C)]`

**机制**：
- `std::mem::size_of::<T>()` — 编译时获取大小
- `std::mem::align_of::<T>()` — 编译时获取对齐
- `#[repr(C)]` — 强制 C 布局
- `offset_of!()` macro (Rust 1.77+) — 获取字段偏移

**能力**：
- ✅ 编译时大小/对齐/偏移
- ✅ `#[repr(C)]` 保证跨平台一致布局
- ❌ 无"类型签名"概念——信息是分散的，不是一个统一字符串
- ❌ 无自动化的跨类型完备比较
- ❌ 需要手动对每个字段调用 `offset_of!`

**TypeLayout vs Rust**：

TypeLayout 的独特贡献是**将分散的布局信息聚合为统一签名**，然后提供**一步比较**。
Rust 有原材料（size、align、offset），但没有聚合工具。

#### 5.2.2 Java `serialVersionUID`

**机制**：每个可序列化类有一个 `serialVersionUID`（long 类型），用于版本控制。

**能力**：
- ✅ 检测类结构变化
- ❌ 手动指定（经常被硬编码为 `1L`）
- ❌ 自动计算版本很脆弱（依赖字段名、方法签名等的哈希）
- ❌ 不提供布局信息
- ❌ Java 没有内存布局的概念（JVM 管理）

**TypeLayout vs Java**：不可比。Java 没有"内存布局"的问题，TypeLayout 解决的是 C++ 特有的底层内存安全问题。

#### 5.2.3 Protocol Buffers / FlatBuffers

**机制**：通过 IDL 文件（.proto / .fbs）定义消息格式，代码生成器生成序列化/反序列化代码。

**能力**：
- ✅ 跨语言、跨平台的数据交换
- ✅ 版本演化（向前/向后兼容）
- ✅ 完善的 schema 演化规则
- ❌ 需要 IDL 文件和代码生成步骤
- ❌ 运行时序列化/反序列化开销
- ❌ 不是零拷贝（Protobuf）/ 有限零拷贝（FlatBuffers）
- ❌ 需要学习 IDL 语言

**TypeLayout vs Protobuf**：

| 维度 | Protobuf | TypeLayout |
|------|----------|-----------|
| 目标 | 跨语言数据交换 | 同语言布局验证 |
| IDL 需求 | ✅ 需要 .proto | ❌ 直接使用 C++ struct |
| 代码生成 | ✅ 需要 protoc | ❌ 无 |
| 运行时开销 | 序列化/反序列化 | 零（编译时） |
| 跨语言 | ✅ | ❌ C++ only |
| 布局验证 | ❌ 不直接 | ✅ 核心功能 |

**结论**：Protobuf 解决"跨语言数据交换"，TypeLayout 解决"同语言布局一致性"。
当用户选择**不使用** Protobuf（因为性能、复杂度或已有 C++ struct）时，
TypeLayout 是验证原生 struct 安全性的工具。

#### 5.2.4 Cap'n Proto

**机制**：零拷贝序列化——数据在内存中的表示就是 wire format。

**能力**：
- ✅ 真正的零拷贝
- ✅ 跨平台（通过固定布局）
- ❌ 需要 IDL 和代码生成
- ❌ 生成的类型不是普通 C++ struct（是封装对象）
- ❌ 不能用于已有的原生 C++ struct

**TypeLayout vs Cap'n Proto**：

Cap'n Proto 的零拷贝要求使用它自己的类型系统。
TypeLayout 的价值是验证**已有的原生 C++ struct** 是否可以零拷贝传输。
当用户已经有大量原生 struct（遗留代码），不想迁移到 Cap'n Proto 时，TypeLayout 是答案。

---

### 5.3 对比分析

#### 5.3.1 多维度对比矩阵

| 方案 | 分析类型 | 布局信息 | 时机 | 完备性 | 自动性 | 开销 | C++ 标准 |
|------|:-------:|:-------:|:---:|:------:|:------:|:---:|:--------:|
| **RTTI** | 名义 | ❌ | 运行时 | — | ✅ | 有 | C++11 |
| **Boost.TypeIndex** | 名义 | ❌ | 编译时+运行时 | — | ✅ | 低 | C++11 |
| **ABI Mangling** | 名义 | ❌ | 链接时 | — | ✅ | 零 | N/A |
| **sizeof/offsetof** | 结构 | 部分 | 编译时 | ❌ 手动 | ❌ | 零 | C++11 |
| **Boost.PFR** | 结构 | 部分 | 编译时 | 部分 | ✅ | 零 | C++17 |
| **ABI Checker** | 全面 | ✅ | 构建后 | ✅ | ✅ | 高 | 外部工具 |
| **Protobuf** | Schema | ✅ | 运行时 | ✅ | ✅ | 有 | IDL |
| **Cap'n Proto** | Schema | ✅ | 编译时 | ✅ | ✅ | 零 | IDL |
| **TypeLayout** | **结构** | **✅** | **编译时** | **✅** | **✅** | **零** | **C++26** |

#### 5.3.2 TypeLayout 的不可替代区间

TypeLayout 占据的是一个**独特的生态位**：

```
                    编译时 ←───────→ 运行时
                       │                │
    完备 ─────── TypeLayout ────────── ABI Checker
    布局信息         │                     │
                     │                     │
    部分 ─────── sizeof/offsetof      RTTI/TypeIndex
    布局信息     Boost.PFR
                     │
    无 ──────────────── ABI Mangling
    布局信息
```

**TypeLayout 的不可替代区间**：

> **编译时 + 完备布局信息 + 自动化 + 零开销 + 原生 C++ struct**

在这个五维组合中，没有任何现有方案能做到：

1. `sizeof/offsetof` — 编译时但不完备、不自动
2. `Boost.PFR` — 编译时自动但不完备（无偏移、无继承、无位域）
3. `ABI Checker` — 完备但不是编译时
4. `Protobuf` — 完备但不是原生 struct
5. `RTTI` — 运行时且无布局信息

**只有 TypeLayout 同时满足全部五项。**

#### 5.3.3 Boost 评审者可能的质疑点

**Q1: "为什么不等 P2996 进标准后再做？"**

A: P2996 提供的是反射原语（`nonstatic_data_members_of` 等），
不提供"聚合为签名并比较"的能力。TypeLayout 是 P2996 的**应用层**，
就像 Boost.Format 是 `printf` 的应用层。两者不冲突。

**Q2: "依赖 P2996 意味着用户量极小"**

A: 短期确实如此。但：
- P2996 是 C++26 最重要的提案之一，预计 2028 年进入主流编译器
- TypeLayout 的跨平台工具链（sig_export + compat_check）已经不需要 P2996
- 先行建立库可以影响 P2996 的实际应用方向

**Q3: "手动 static_assert 已经够用了"**

A: 对于 5 个字段的简单 struct，手动够用。但：
- 20+ 字段的复杂 struct，手动断言维护成本 O(n)
- 新增/删除字段时，手动断言需要同步更新
- 跨类型比较（"A 和 B 布局相同吗？"）用手动断言极其繁琐
- TypeLayout 提供 O(1) 的自动化方案

**Q4: "Boost.PFR 已经做了类似的事"**

A: Boost.PFR 获取字段类型和数量，但：
- 不提供偏移信息
- 不支持继承、多态、位域
- 不提供字段名
- 不提供"签名比较"功能
- TypeLayout 是 Boost.PFR 的**严格超集**（在 P2996 可用时）

**Q5: "用 Protobuf/FlatBuffers 不就行了？"**

A: Protobuf 需要：
- IDL 文件 + 代码生成器
- 迁移所有现有 struct 到 proto 消息
- 运行时序列化开销

TypeLayout 的目标用户是**已有大量原生 C++ struct 且不想迁移**的项目。
这在游戏、嵌入式、高频交易等领域是常态。

---

### 5.4 TypeLayout 的独特价值定位

**一句话定位**：

> **TypeLayout 是 C++ 的编译时类型布局验证器——基于 P2996 反射，零开销，完备，自动化。**

**差异化公式**：

```
TypeLayout = sizeof/offsetof 的自动化
           + Boost.PFR 的完备化（偏移 + 继承 + 位域 + 名称）
           + ABI Checker 的编译时化
           + Protobuf 的零 IDL 化
```

**不可替代的核心场景**：

| 场景 | 为什么 TypeLayout 不可替代 |
|------|--------------------------|
| 遗留 struct 的 IPC 安全验证 | 不想迁移到 Protobuf，手动 assert 维护不了 |
| 插件 ABI 兼容检查 | RTTI 不检查布局，ABI Checker 不是编译时 |
| 跨编译单元 ODR 检测 | 编译器/链接器做不到，TypeLayout 是唯一方案 |
| 文件格式版本自动检测 | 手动版本号易遗漏，TypeLayout 自动完备 |

**最终评估**：

TypeLayout 的价值不是"做到了别人做不到的事"（某些维度 ABI Checker 更全面），
而是"在一个独特的维度组合中做到了最优"：

> **编译时 × 完备 × 自动 × 零开销 × 原生 struct = TypeLayout 独占**
