# TypeLayout 推荐方案设计文档

---

## 一、库定位

```
TypeLayout —— C++ 编译期类型内存布局签名库

核心使命：
  生成确定性的、可比较的类型布局签名，
  用于跨编译单元、跨库边界、跨平台的布局兼容性检测。
```

---

## 二、架构分层

```
┌─────────────────────────────────────────────────────┐
│                     Tool 层                          │
│                                                      │
│   classify<T>                                        │
│     消费 layout_traits + std type_traits             │
│     输出安全等级判定                                   │
│     定位：便利工具，非核心承诺                          │
│                                                      │
├──────────────────────────────────────────────────────┤
│                     Core 层                          │
│                                                      │
│   ┌──────────────────────────────────────────────┐  │
│   │  layout_traits<T>                            │  │
│   │                                              │  │
│   │  遍历字段，提取结构化布局属性                   │  │
│   │                                              │  │
│   │  主产品：                                     │  │
│   │    signature        — 布局签名字符串/hash      │  │
│   │                                              │  │
│   │  自然副产品（遍历过程中零成本获得）：            │  │
│   │    has_pointer       — 是否包含指针字段        │  │
│   │    has_padding       — 是否存在填充字节        │  │
│   │    has_opaque        — 是否包含不可分析的字段   │  │
│   │    is_platform_variant — 布局是否因平台而异    │  │
│   │    field_count       — 字段数量               │  │
│   │    total_size        — 总大小                  │  │
│   │    alignment         — 对齐要求               │  │
│   │                                              │  │
│   └──────────────────────────────────────────────┘  │
│                                                      │
│   signature_compare<T, U>                            │
│     比较两个类型的签名是否一致                         │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## 三、Core 层详细设计

### 3.1 layout_traits\<T\>

```cpp
template <typename T>
struct layout_traits {
    // ═══════════════════════════════════════
    // 主产品：签名
    // ═══════════════════════════════════════
    
    /// 布局签名，编码了所有字段的类型标签、offset、size、alignment
    /// 格式："{field_tag@offset, ...}=total_size:alignment"
    static constexpr auto signature = /* 编译期生成 */;
    
    // ═══════════════════════════════════════
    // 自然副产品：遍历过程中零成本获得的属性
    // ═══════════════════════════════════════
    
    /// 是否包含指针类型字段（含成员指针、函数指针）
    /// 来源：遍历字段分类时，遇到指针即标记
    static constexpr bool has_pointer = /* ... */;
    
    /// 是否存在填充字节（相邻字段 offset 间有间隙，或尾部有填充）
    /// 来源：计算 offset 和 size 时，自然可判断
    static constexpr bool has_padding = /* ... */;
    
    /// 是否包含不可分析的字段（无法递归展开的类型）
    /// 来源：遍历遇到无法反射的类型时标记
    static constexpr bool has_opaque = /* ... */;
    
    /// 布局是否因平台而异（含 long、long double、指针等平台相关类型）
    /// 来源：字段分类时识别平台相关的基础类型
    static constexpr bool is_platform_variant = /* ... */;
    
    /// 字段数量
    static constexpr std::size_t field_count = /* ... */;
    
    /// 总大小（字节）
    static constexpr std::size_t total_size = sizeof(T);
    
    /// 对齐要求（字节）
    static constexpr std::size_t alignment = alignof(T);
};
```

### 3.2 签名比较

```cpp
template <typename T, typename U>
struct signature_compare {
    static constexpr bool value = 
        layout_traits<T>::signature == layout_traits<U>::signature;
};

template <typename T, typename U>
inline constexpr bool signature_compare_v = signature_compare<T, U>::value;
```

### 3.3 核心层边界判定标准

```
一个功能是否属于核心层，唯一的判定问题：

  "去掉它，签名的生成和比较还能正确工作吗？"

  能   → 不属于核心
  不能 → 属于核心

自然副产品的特殊地位：
  它们本身不影响签名的正确性（去掉它们签名仍然正确），
  但它们是签名生成过程中零成本获得的信息，
  暴露它们不增加核心层的复杂度，
  且对 tool 层和用户有明确价值。

  因此：暴露自然副产品，但不为它们扩展遍历逻辑。
  如果某个属性需要额外的遍历或分析才能获得，它不属于核心层。
```

---

## 四、Tool 层详细设计

### 4.1 classify\<T\>

```cpp
/// 安全等级枚举
enum class SafetyLevel {
    /// 可安全 memcpy，无指针、无填充、平凡可拷贝、布局平台无关
    TrivialSafe,
    
    /// 可 memcpy 但有填充（可能泄露未初始化内存）
    PaddingRisk,
    
    /// 包含指针，字节级复制会导致语义错误（悬垂指针、重复释放）
    PointerRisk,
    
    /// 布局因平台而异，跨平台传输不安全
    PlatformVariant,
    
    /// 包含不可分析的字段，无法判定安全性
    Opaque,
};

template <typename T>
struct classify {
    static constexpr SafetyLevel value = /* 基于以下属性判定 */;
    
    // 判定逻辑：
    //   if (layout_traits<T>::has_opaque)          → Opaque
    //   if (layout_traits<T>::has_pointer)          → PointerRisk
    //   if (layout_traits<T>::is_platform_variant)  → PlatformVariant
    //   if (layout_traits<T>::has_padding)           → PaddingRisk
    //   if (!std::is_trivially_copyable_v<T>)       → PointerRisk (保守)
    //   else                                        → TrivialSafe
};

template <typename T>
inline constexpr SafetyLevel classify_v = classify<T>::value;
```

### 4.2 classify 的依赖关系

```
classify 的输入：
  1. layout_traits<T> 的自然副产品（has_pointer, has_padding, ...）
     → 来自 Core 层，依赖关系真实
  2. std::is_trivially_copyable_v<T> 等标准 type_traits
     → 来自标准库，classify 层自行引入

classify 不依赖：
  ✗ 签名字符串本身
  ✗ 签名比较结果
  ✗ 任何需要绕过核心层直接调反射的能力

这使得 classify 是核心层的正当下游消费者，
而不是签名的下游、也不是核心的平级替代。
```

### 4.3 classify 的定位声明

```
classify 是便利工具，不是核心承诺。

含义：
  1. 用户可以不使用 classify，直接读取 layout_traits 的属性自行判断
  2. classify 的安全等级划分是一种"推荐策略"，不是唯一正确的策略
  3. classify 的判定逻辑可能随版本演进调整
  4. 核心层的 API 稳定性高于 classify 的 API 稳定性
```

---

## 五、用户使用场景

### 5.1 场景 A：纯签名比较（只用核心层）

```cpp
// 跨 DLL 边界验证类型布局一致性
static_assert(
    signature_compare_v<MyStruct_v1, MyStruct_v2>,
    "类型布局不兼容，无法跨边界传递"
);
```

### 5.2 场景 B：直接使用属性（核心层属性 + 用户自定义策略）

```cpp
template <typename T>
void serialize(const T& obj, std::span<std::byte> buffer) {
    // 用户自定义安全策略，不使用 classify
    static_assert(!layout_traits<T>::has_pointer,
        "不允许序列化含指针的类型");
    static_assert(!layout_traits<T>::has_padding,
        "不允许序列化含填充的类型（信息泄露风险）");
    static_assert(std::is_trivially_copyable_v<T>,
        "类型必须可平凡拷贝");
    
    std::memcpy(buffer.data(), &obj, sizeof(T));
}
```

### 5.3 场景 C：使用 classify（tool 层便利工具）

```cpp
template <typename T>
void safe_serialize(const T& obj, std::span<std::byte> buffer) {
    static_assert(
        classify_v<T> == SafetyLevel::TrivialSafe,
        "类型不满足 zero-copy 序列化的安全要求"
    );
    
    std::memcpy(buffer.data(), &obj, sizeof(T));
}
```

### 5.4 场景 D：签名 + classify 联合使用

```cpp
template <typename T>
void cross_boundary_transfer(const T& obj, void* dest) {
    // 1. 安全性检查
    static_assert(
        classify_v<T> == SafetyLevel::TrivialSafe,
        "类型不安全，不能跨边界传输"
    );
    
    // 2. 运行时布局兼容性检查
    auto remote_sig = get_remote_signature();  // 从另一侧获取签名
    assert(remote_sig == layout_traits<T>::signature 
           && "远端类型布局不兼容");
    
    std::memcpy(dest, &obj, sizeof(T));
}
```

---

## 六、不做什么（明确的边界排除）

```
TypeLayout 不做：
  ✗ 序列化框架（只提供判定信息，不提供序列化实现）
  ✗ 类型转换（不提供类型之间的转换机制）
  ✗ 运行时反射（所有分析在编译期完成）
  ✗ ABI 兼容性保证（签名描述布局，不保证 ABI 兼容）
  ✗ 自定义序列化策略（classify 只给出安全等级，不决定序列化方式）
  ✗ is_trivially_copyable 等标准已有能力的重复实现
```

---

## 七、layout_traits 自然副产品的准入标准

```
一个属性能作为 layout_traits 的自然副产品暴露，当且仅当：

  1. 它在签名生成的遍历过程中已经被计算或可零成本获得
  2. 它不需要引入额外的遍历逻辑或分析能力
  3. 它描述的是客观的布局事实，不是主观的安全判定

满足条件的：
  ✅ has_pointer        （遍历时已识别字段类型）
  ✅ has_padding         （遍历时已计算 offset 和 size）
  ✅ has_opaque          （遍历时已识别不可分析的类型）
  ✅ is_platform_variant （遍历时已识别平台相关类型）
  ✅ field_count         （遍历时自然得到）
  ✅ total_size          （sizeof，零成本）
  ✅ alignment           （alignof，零成本）

不满足条件的：
  ❌ is_trivially_copyable  （标准库已有，且不是布局分析的产物）
  ❌ safety_level           （主观判定，不是客观布局事实）
  ❌ can_memcpy             （主观判定）
  ❌ serialization_strategy （策略选择，不是事实描述）
```

---

## 八、总结

```
Core 层（layout_traits + signature + signature_compare）：
  ● 签名的生成与比较 —— 核心使命
  ● 自然副产品属性的暴露 —— 零成本附带
  ● 边界清晰，职责单一
  ● API 稳定性最高

Tool 层（classify）：
  ● 消费 layout_traits 属性 + 标准 type_traits
  ● 输出安全等级判定
  ● 便利工具，非核心承诺
  ● 用户可绕过，直接使用属性自行判断
  ● API 可随版本演进调整

依赖方向：
  Tool 层 → Core 层 → 编译器反射 / 标准库
  单向依赖，无循环，核心层不知道 tool 层的存在。
```