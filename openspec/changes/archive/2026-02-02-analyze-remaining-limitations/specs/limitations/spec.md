# TypeLayout 兼容性分析与限制规范

## ADDED Requirements

### Requirement: 签名驱动的兼容性模型

TypeLayout 的序列化兼容性 SHALL 完全基于签名比较，不区分"同编译器"或"跨编译器"场景。

#### Scenario: 签名匹配即兼容
- **GIVEN** 两个端点（序列化端和反序列化端）
- **WHEN** 两端对同一类型生成的签名相同
- **THEN** 类型 SHALL 被视为兼容
- **AND** memcpy 序列化 SHALL 是安全的

#### Scenario: 签名不匹配即不兼容
- **GIVEN** 两个端点
- **WHEN** 两端对同一类型生成的签名不同
- **THEN** 类型 SHALL 被视为不兼容
- **AND** 序列化检查 SHALL 失败

#### Scenario: 签名包含完整布局信息
- **GIVEN** 任意类型 T
- **WHEN** 生成布局签名
- **THEN** 签名 SHALL 包含足够信息以确定内存布局：
  - 成员偏移量（包括虚拟基类偏移）
  - 成员大小和对齐
  - 位域位置和宽度

---

### Requirement: 虚拟继承完全兼容

虚拟继承类型 SHALL 被视为 TypeLayout 完全兼容，因为签名中已包含偏移信息。

#### Scenario: 虚拟基类签名包含偏移
- **GIVEN** 一个包含虚拟继承的类型
- **WHEN** 生成布局签名
- **THEN** 签名 SHALL 包含虚拟基类的实际偏移量
- **AND** 格式为 `@offset[vbase]:class{...}`

#### Scenario: 虚拟继承跨平台检测
- **GIVEN** 相同源码在不同编译器上编译
- **WHEN** 虚拟基类偏移不同（因 ABI 差异）
- **THEN** 生成的签名 SHALL 不同
- **AND** 签名比较 SHALL 检测到不兼容
- **EXAMPLE** GCC: `@8[vbase]:A{...}` vs MSVC: `@12[vbase]:A{...}` → 不匹配

#### Scenario: 虚拟继承序列化检查
- **GIVEN** 一个包含虚拟基类的类型
- **WHEN** 检查 `is_serializable_v<T>`
- **THEN** 结果 SHALL 基于签名是否可比较（不因虚拟继承而拒绝）
- **AND** 虚拟继承本身不是序列化阻止因素

---

### Requirement: 标准库类型兼容性策略

TypeLayout SHALL 定义明确的标准库类型支持策略，区分"专用特化"、"结构体反射"和"不支持"三种处理方式。

#### Scenario: std::tuple 处理策略
- **GIVEN** 用户请求 `std::tuple<T...>` 的布局签名
- **WHEN** 类型被反射
- **THEN** 实现 SHALL 使用通用结构体反射
- **AND** 签名 SHALL 反映实际的内存布局（可能包含 EBO 优化）

#### Scenario: std::variant 处理策略
- **GIVEN** 用户请求 `std::variant<T...>` 的布局签名
- **WHEN** 类型被反射
- **THEN** 签名 SHALL 包含内部标签和联合体结构
- **AND** `is_serializable_v` SHALL 为 `false`（因为活动索引是运行时状态）

#### Scenario: std::optional 处理策略
- **GIVEN** 用户请求 `std::optional<T>` 的布局签名
- **WHEN** 类型被反射
- **THEN** 签名 SHALL 反映内部 engaged 标志和值存储
- **AND** `is_serializable_v` SHALL 为 `false`（因为 engaged 状态是运行时状态）

### Requirement: 位域兼容性

位域类型 SHALL 遵循签名驱动的兼容性模型，签名中包含位域位置信息。

#### Scenario: 位域签名包含位位置
- **GIVEN** 一个包含位域成员的结构体
- **WHEN** 生成布局签名
- **THEN** 签名 SHALL 包含位域的位位置和宽度
- **AND** 不同编译器的位域布局差异 SHALL 反映在签名中

#### Scenario: 位域序列化检查
- **GIVEN** 一个包含位域的结构体
- **WHEN** 检查 `is_serializable_v<T>`
- **THEN** 结果 SHALL 基于签名是否可比较（不因位域而拒绝）
- **AND** 位域本身不是序列化阻止因素

#### Scenario: 位域文档警告
- **GIVEN** 用户阅读位域文档
- **THEN** 文档 SHALL 说明：
  - 签名准确反映当前编译器的位域布局
  - 签名比较会自动检测不同编译器的布局差异
  - 建议跨平台协议使用显式位操作

---

## 深度技术分析

### 问题 1: 虚拟继承偏移

#### 1.1 根本原因

虚拟继承的核心问题在于**虚拟基类的实际偏移在运行时是动态的**，取决于对象的最终派生类型。

```cpp
struct A { int a; };
struct B : virtual A { int b; };
struct C : virtual A { int c; };
struct D : B, C { int d; };  // 菱形继承

// 问题：
// - 在 B 的上下文中，A 的偏移是 X
// - 在 D 的上下文中，A 的偏移可能是 Y (≠ X)
```

#### 1.2 P2996 行为分析

P2996 `offset_of` 对虚拟基类的行为：

| 场景 | `offset_of` 返回值 | 说明 |
|------|-------------------|------|
| 非虚拟基类 | 静态固定偏移 | 可靠 |
| 虚拟基类 (最终派生类) | 静态偏移 | 可靠 |
| 虚拟基类 (中间类) | **可能是占位值** | 不可靠 |

Bloomberg P2996 实现中，对虚拟基类的 `offset_of` 可能返回：
- 该类作为最终派生类时的偏移
- 或编译器内部的占位值

#### 1.3 ABI 差异

| ABI | 虚拟基类偏移机制 |
|-----|-----------------|
| Itanium (GCC/Clang) | 通过 vtable 中的 vbase offset 查找 |
| MSVC | 通过 vbptr (虚基类指针) 查找 |

两种机制都意味着**实际偏移是运行时计算的**。

#### 1.4 解决方案：签名驱动

**核心认知**：签名已包含偏移信息，无需特殊处理。

```cpp
// GCC 上虚拟继承类型的签名
"class[s:16,a:8]{@8[vbase]:A{x:i32}; d:i32}"

// MSVC 上相同类型的签名
"class[s:20,a:8]{@12[vbase]:A{x:i32}; d:i32}"

// 签名不同 → 自动检测到不兼容
```

**签名比较机制**：
- 两端各自生成签名
- 签名相同 → 布局相同 → memcpy 安全
- 签名不同 → 布局不同 → 序列化失败

**不需要特殊拒绝虚拟继承**，因为：
1. 签名包含精确偏移，布局差异会体现在签名中
2. 虚拟继承本身没有运行时状态问题
3. memcpy 一个虚拟继承对象是完全安全的（只要布局相同）

---

### 问题 2: 标准库类型支持

#### 2.1 std::tuple 分析

**内部实现复杂性**:

```cpp
// 典型的递归继承实现 (libstdc++)
template<typename... Types>
class tuple : private tuple_impl<0, Types...> {
    // Empty Base Optimization (EBO) 可能被应用
};

// 或者扁平存储实现 (libc++)
template<typename... Types>
class tuple {
    __tuple_storage<Types...> _storage;
};
```

**问题**:
- 不同标准库实现布局不同
- EBO 应用与否影响大小
- 成员顺序可能是反向的

**建议**: 不添加专用特化，原因：
1. 布局差异太大，无法提供稳定签名
2. 通用结构体反射已能正确显示实际布局
3. 用户如需跨平台一致性，应使用自定义 POD 结构

#### 2.2 std::variant 分析

**内部实现**:

```cpp
template<typename... Types>
class variant {
    union { Types... _storage; };  // 所有类型的联合
    size_t _index;                  // 当前活动索引
};
```

**序列化问题**:
- `_index` 决定哪个成员有效
- memcpy 无法保证 `_index` 与数据一致性
- 反序列化后可能访问错误的成员

**建议**: 
1. 通用反射已足够显示内部结构
2. `is_serializable_v` 返回 `false`（已实现）
3. 添加文档说明 variant 的序列化风险

#### 2.3 std::optional 分析

**内部实现**:

```cpp
template<typename T>
class optional {
    union { char _dummy; T _value; };
    bool _engaged;  // 或者使用哨兵值
};
```

**序列化问题**:
- `_engaged` 是运行时状态
- memcpy 后 `_engaged` 和 `_value` 可能不一致

**建议**: 同 `std::variant`

#### 2.4 总结

| 类型 | 专用特化 | 序列化 | 建议 |
|------|---------|--------|------|
| `std::tuple` | ❌ 不添加 | 允许（如果元素可序列化） | 依赖通用反射 |
| `std::variant` | ❌ 不添加 | ❌ 拒绝 | 添加文档 |
| `std::optional` | ❌ 不添加 | ❌ 拒绝 | 添加文档 |

---

### 问题 3: 位域跨平台布局

#### 3.1 C++ 标准规定

C++ 标准 [class.bit] 规定：

> Allocation of bit-fields within a class object is implementation-defined.
> Alignment of bit-fields is implementation-defined.

标准明确将位域布局定义为**实现定义**，不是未定义也不是未指定。

#### 3.2 编译器差异对比

| 特性 | GCC/Clang (Itanium) | MSVC |
|------|---------------------|------|
| 默认打包方向 | LSB 优先 | LSB 优先 |
| 跨存储单元 | 允许（取决于对齐） | 不允许，强制新单元 |
| 零宽度位域 | 对齐到单元边界 | 对齐到单元边界 |
| 无名位域 | 占用空间 | 占用空间 |
| `#pragma pack` 影响 | 影响存储单元大小 | 同 |

**关键差异示例**:

```cpp
struct BitfieldExample {
    uint32_t a : 17;
    uint32_t b : 17;
};

// GCC/Clang: 可能跨两个 uint32_t，sizeof = 8
// MSVC: b 强制在新 uint32_t 开始，sizeof = 8
// 但位域的实际位位置可能不同！
```

#### 3.3 序列化风险

1. **相同源码，不同布局**: 同一结构体定义在不同编译器上可能产生不同的位布局
2. **字节序问题**: 位域在不同字节序平台上的行为不一致
3. **调试困难**: 位级错误难以诊断

#### 3.4 最佳实践建议

**对于需要跨平台的二进制协议**:

```cpp
// ❌ 不要这样做
struct BadPacket {
    uint32_t version : 4;
    uint32_t flags : 12;
    uint32_t length : 16;
};

// ✅ 使用显式位操作
struct GoodPacket {
    uint32_t header;  // 手动打包/解包
    
    uint8_t version() const { return header & 0xF; }
    uint16_t flags() const { return (header >> 4) & 0xFFF; }
    uint16_t length() const { return header >> 16; }
    
    void set_version(uint8_t v) { header = (header & ~0xF) | (v & 0xF); }
    // ... 其他 setter
};
```

**TypeLayout 的角色**:

1. 准确反映**当前编译器**的位域布局 ✅
2. 签名包含位域位置，自动检测跨平台不兼容 ✅
3. 文档中提供跨平台协议的最佳实践建议 ✅

---

## 结论与建议

### 修正后的兼容性模型

**核心原则**：签名相同 ⟺ 内存布局相同 ⟺ memcpy 安全

| 类型特征 | TypeLayout 签名 | 序列化 | 原因 |
|---------|----------------|--------|------|
| 虚拟继承 | ✅ 包含偏移信息 | ✅ 允许 | 签名比较自动检测不兼容 |
| 位域 | ✅ 包含位位置 | ✅ 允许 | 签名比较自动检测不兼容 |
| std::tuple | ✅ 反映实际布局 | ✅ 允许 | 签名比较自动检测不兼容 |
| std::variant | ✅ 正确生成 | ❌ 拒绝 | **运行时状态问题** |
| std::optional | ✅ 正确生成 | ❌ 拒绝 | **运行时状态问题** |
| 指针/引用 | N/A | ❌ 拒绝 | 地址跨进程无意义 |
| 虚函数 | N/A | ❌ 拒绝 | vptr 跨进程无意义 |

### 建议行动

| 问题 | 类型 | 建议行动 |
|------|------|---------|
| 虚拟继承 | ✅ 已解决 | 签名包含偏移，无需特殊处理 |
| 位域 | ✅ 已解决 | 签名包含位位置，无需特殊处理 |
| std::tuple | ✅ 已解决 | 签名反映实际布局，无需特殊处理 |
| std::variant | 运行时状态 | 保持序列化拒绝 + 文档说明 |
| std::optional | 运行时状态 | 保持序列化拒绝 + 文档说明 |

### 关键认知

1. **签名是兼容性的唯一标准** - 不区分"同编译器"和"跨编译器"
2. **签名包含完整布局信息** - 偏移、大小、位位置等
3. **签名比较自动检测不兼容** - 虚拟继承、位域的平台差异通过签名暴露
4. **只有运行时状态类型需要拒绝** - variant/optional 的问题不是布局，是语义

### 为什么 std::variant/optional 特殊？

这不是布局问题，是**语义问题**：

```cpp
std::variant<int, std::string> v1 = 42;  // _index = 0
std::variant<int, std::string> v2 = "hello";  // _index = 1

memcpy(&v2, &v1, sizeof(v1));
// 现在 v2._index = 0，但内部的 string 没有被析构！
// 后续任何操作都是未定义行为
```

问题在于：**memcpy 无法正确处理对象生命周期语义**。

### 为什么虚拟继承和位域安全？

它们没有运行时状态问题，只有布局差异：

```cpp
struct VBase { int x; };
struct D : virtual VBase { int d; };

D obj1, obj2;
memcpy(&obj2, &obj1, sizeof(D));  // 完全安全！
// 签名相同 → 布局相同 → memcpy 安全
```

布局差异会反映在签名中，签名比较会自动处理。
