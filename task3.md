# Serialization Free 方案设计文档（纯签名，无哈希）

---

## 1. 概述

### 1.1 目标

基于 TypeLayout 的编译期类型签名（`FixedString`），提供零开销的 serialization-free 传输判定。不使用哈希，所有兼容性判定基于签名字符串精确匹配。

### 1.2 核心理念

```text
Serialization Free = 不需要编解码，直接 memcpy 传输

  发送端：  memcpy(buf, &obj, sizeof(T));   send(buf, sizeof(T));
  接收端：  recv(buf, sizeof(T));           memcpy(&obj, buf, sizeof(T));

  零解析、零字段遍历、零编解码。
  Padding 字节一起传输，不关心其内容。
```

### 1.3 判定公式

```text
serialization_free<T> =
    ① trivially_copyable(T)              // memcpy 不破坏对象模型
    ② && !has_pointer(T)                  // 无地址空间依赖
    ③ && signature_match(local, remote)   // 两端签名字符串精确相等

③ 自动覆盖：
  - padding 差异（offset 不同 → 签名不同）
  - platform variant（long/size_t 宽度不同 → 签名不同）
  - 字节序差异（架构前缀编码字节序）
  - alignment 差异（alignof 不同 → 签名不同）
```

### 1.4 不使用哈希的理由

```text
1. 零碰撞：签名精确匹配，不存在假阳性
   → serialization free 的假阳性后果是 silent data corruption
   → 任何非零碰撞风险不可接受

2. 自文档化：签名字符串本身就是诊断信息
   → 签名不匹配时直接对比字符串即可定位差异
   → 不需要额外传输/存储诊断信息

3. 性能无关：签名比较只在握手阶段发生一次
   → 数据传输路径上不存在签名比较
   → 字符串比较 vs 整数比较的差异在此场景下无意义

4. 简化设计：去掉哈希层，减少一个概念
   → 签名即身份，不需要中间表示
```

---

## 2. 签名系统

### 2.1 签名格式规范

```text
完整签名 = 架构前缀 + 类型签名

架构前缀：
  "[64-le]"  — 64位小端
  "[64-be]"  — 64位大端
  "[32-le]"  — 32位小端
  "[32-be]"  — 32位大端

类型签名语法（BNF-like）：

  type_sig  ::= primitive_sig | struct_sig | array_sig | enum_sig | union_sig | opaque_sig

  primitive_sig ::= kind_tag
    kind_tag ∈ { "b", "i8", "u8", "i16", "u16", "i32", "u32",
                 "i64", "u64", "f32", "f64", "f128", "c8", "c16", "c32" }

  struct_sig ::= "{" field_list "|" total_size "|" alignment "}"
  field_list ::= field ("," field)*
  field      ::= type_sig "@" offset

  array_sig  ::= type_sig "[" count "]"

  enum_sig   ::= "E(" underlying_type_sig ")"

  union_sig  ::= "U{" variant_list "|" total_size "|" alignment "}"
  variant_list ::= type_sig ("," type_sig)*

  opaque_sig ::= "O(" tag "|" size "|" alignment ")"
```

### 2.2 签名示例

```text
int32_t                    → "i32"
double                     → "f64"

struct Point {
    int32_t x;             // offset 0
    int32_t y;             // offset 4
};                         → "{i32@0,i32@4|8|4}"

struct Sensor {
    char     id;           // offset 0
    // 3 bytes padding
    int32_t  value;        // offset 4
    double   timestamp;    // offset 8
};                         → "{c8@0,i32@4,f64@8|16|8}"

int32_t data[4];           → "i32[4]"

enum class Color : uint8_t { R, G, B };
                           → "E(u8)"

// opaque: 32字节密钥块，8字节对齐
                           → "O(AesKey256|32|8)"

完整签名（含架构前缀）：
  "[64-le]{c8@0,i32@4,f64@8|16|8}"
```

### 2.3 签名的编译期计算

```cpp
// 所有签名在编译期计算为 FixedString<N>
// FixedString 是编译期字符串，N 在编译期确定

template <typename T>
struct TypeSignature {
    static constexpr auto value = compute_signature<T>();
    // value 的类型是 FixedString<N>，N 自动推导
};

// 架构前缀
constexpr auto arch_prefix = make_arch_prefix();
// → FixedString，内容如 "[64-le]"

// 完整签名
template <typename T>
constexpr auto full_signature = arch_prefix + TypeSignature<T>::value;
```

---

## 3. TypeLayout 核心层

### 3.1 layout_traits 主模板

```cpp
namespace boost::typelayout {

template <typename T>
struct layout_traits {
    // ── 签名（编译期 FixedString）──
    static constexpr auto signature = full_signature<T>;

    // ── 指针检测 ──
    static constexpr bool has_pointer = compute_has_pointer<T>();

    // ── 基本属性 ──
    static constexpr std::size_t total_size = sizeof(T);
    static constexpr std::size_t alignment  = alignof(T);

    // ── trivially copyable ──
    static constexpr bool trivially_copyable = std::is_trivially_copyable_v<T>;

    // ── 本端 serialization-free 可行性（不含远端签名匹配）──
    static constexpr bool local_serialization_free =
        trivially_copyable && !has_pointer;
};

} // namespace boost::typelayout
```

### 3.2 `has_pointer` 递归检测

```text
has_pointer<T> 的判定逻辑：

  基本类型（int, float, char, enum...） → false
  指针类型（T*, void*, 成员指针）       → true
  引用类型（T&, T&&）                   → true（不应出现在可序列化类型中）
  数组类型（T[N]）                      → has_pointer<T>
  结构体类型                            → any_of(fields, has_pointer<field_type>)
  union 类型                            → any_of(variants, has_pointer<variant_type>)
  opaque 类型                           → 用户声明值（默认 true，保守策略）
  
注意事项：
  ● std::string         → 包含 char* → has_pointer = true
  ● std::vector<int>    → 包含 int*  → has_pointer = true
  ● std::array<int, 4>  → 不含指针   → has_pointer = false
  ● 嵌套结构体          → 递归检测
```

---

## 4. Opaque 类型支持

### 4.1 设计原理

```text
Opaque 类型 = 内部布局对 TypeLayout 不可见的类型

特征：
  ✓ sizeof 已知
  ✓ alignof 已知
  ✗ 字段列表未知（无法反射展开）
  ? trivially_copyable → 需要用户断言
  ? has_pointer → 需要用户断言

签名格式：
  "O(" tag "|" size "|" alignment ")"

  tag: 用户提供的唯一标识符（语义名称）
  size: sizeof 值
  alignment: alignof 值

  示例：
    O(AesKey256|32|8)     — 32字节密钥块，8对齐
    O(SensorRaw|64|4)     — 64字节传感器原始数据，4对齐
    O(HwRegBlock|256|16)  — 256字节硬件寄存器映射，16对齐
```

### 4.2 Opaque 注册宏

```cpp
// ═══════════════════════════════════════════════════════════
// TYPELAYOUT_REGISTER_OPAQUE 宏
// ═══════════════════════════════════════════════════════════
//
// 用户声明 opaque 类型的 TypeLayout 属性。
// TypeLayout 不会反射展开此类型，而是使用用户提供的信息生成签名。
//
// 参数：
//   Type           — 类型名
//   Tag            — 语义标签字符串（用于签名生成，必须全局唯一）
//   HasPointer     — 是否包含指针（true/false）
//
// sizeof 和 alignof 自动从类型推导。
//
// 安全责任：
//   用户保证：
//     1. Tag 在所有注册的 opaque 类型中唯一
//     2. HasPointer 的声明与实际一致
//     3. 类型是 trivially_copyable（否则 memcpy 不安全）
//     4. 两端使用相同版本的 opaque 定义（布局一致）

#define TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)             \
namespace boost::typelayout {                                         \
    template <>                                                       \
    struct TypeSignature<Type> {                                       \
        static constexpr auto value =                                 \
            FixedString("O(") + FixedString(Tag)                      \
            + FixedString("|") + to_fixed_string(sizeof(Type))        \
            + FixedString("|") + to_fixed_string(alignof(Type))       \
            + FixedString(")");                                       \
    };                                                                \
    template <>                                                       \
    struct layout_traits<Type> {                                      \
        static constexpr auto signature = arch_prefix                 \
                                        + TypeSignature<Type>::value;  \
        static constexpr bool has_pointer = (HasPointer);             \
        static constexpr std::size_t total_size = sizeof(Type);       \
        static constexpr std::size_t alignment  = alignof(Type);      \
        static constexpr bool trivially_copyable =                    \
            std::is_trivially_copyable_v<Type>;                       \
        static constexpr bool local_serialization_free =              \
            trivially_copyable && !has_pointer;                       \
    };                                                                \
}

// ═══════════════════════════════════════════════════════════
// 使用示例
// ═══════════════════════════════════════════════════════════

// 32字节密钥块，不含指针
struct AesKey256 { unsigned char data[32]; };
TYPELAYOUT_REGISTER_OPAQUE(AesKey256, "AesKey256", false)
// 签名: "[64-le]O(AesKey256|32|1)"

// 硬件寄存器块，不含指针
struct alignas(16) HwRegBlock { char regs[256]; };
TYPELAYOUT_REGISTER_OPAQUE(HwRegBlock, "HwRegBlock", false)
// 签名: "[64-le]O(HwRegBlock|256|16)"

// FFI 不透明句柄，包含指针
struct LibHandle { void* impl; int flags; };
TYPELAYOUT_REGISTER_OPAQUE(LibHandle, "LibHandle", true)
// 签名: "[64-le]O(LibHandle|16|8)"
// local_serialization_free = false（has_pointer = true）
```

### 4.3 Opaque 类型在签名中的嵌套

```text
当 opaque 类型作为另一个结构体的字段时：

struct Packet {
    uint32_t   seq;         // offset 0
    AesKey256  key;         // offset 4
    uint32_t   crc;         // offset 36
};

签名: "{u32@0,O(AesKey256|32|1)@4,u32@36|40|4}"

签名中 opaque 部分 "O(AesKey256|32|1)" 整体作为字段类型签名嵌入。
接收端的相同 opaque 类型必须注册相同的 Tag、相同的 size 和 alignment，
否则签名不匹配 → 不能 serialization free。
```

---

## 5. Serialization Free Trait 层

### 5.1 单端判定（编译期）

```cpp
namespace boost::typelayout {

// ═══════════════════════════════════════════════════════════
// is_local_serialization_free<T>
// ═══════════════════════════════════════════════════════════
// 编译期判定：本端是否满足 serialization free 的前提条件。
// 不涉及远端签名比较（远端信息编译期不可用）。

template <typename T>
struct is_local_serialization_free
    : std::bool_constant<layout_traits<T>::local_serialization_free> {};

template <typename T>
inline constexpr bool is_local_serialization_free_v =
    is_local_serialization_free<T>::value;

// ═══════════════════════════════════════════════════════════
// serialization_free_assert<T>
// ═══════════════════════════════════════════════════════════
// 编译期断言：如果类型不满足本端条件，给出清晰错误信息。

template <typename T>
constexpr void serialization_free_assert() {
    static_assert(std::is_trivially_copyable_v<T>,
        "Serialization-free requires trivially_copyable. "
        "Type has non-trivial copy/move/destructor.");

    static_assert(!layout_traits<T>::has_pointer,
        "Serialization-free requires no pointers. "
        "Type contains pointer or reference members "
        "that are address-space dependent.");
}

} // namespace boost::typelayout
```

### 5.2 双端判定（运行时 — 握手阶段）

```cpp
namespace boost::typelayout {

// ═══════════════════════════════════════════════════════════
// SignatureRegistry
// ═══════════════════════════════════════════════════════════
// 运行时签名注册表。
// 在握手阶段，双方交换各自关心类型的签名字符串，
// 注册到本地表中，用于运行时查询兼容性。

class SignatureRegistry {
public:
    // 注册本端类型签名
    template <typename T>
    void register_local() {
        static_assert(is_local_serialization_free_v<T>,
            "Only locally serialization-free types can be registered.");

        auto key = type_id<T>();
        local_signatures_[key] = std::string_view(
            layout_traits<T>::signature.data(),
            layout_traits<T>::signature.size()
        );
    }

    // 记录远端发来的签名
    void register_remote(std::string_view type_name,
                         std::string_view remote_sig) {
        remote_signatures_[std::string(type_name)] = std::string(remote_sig);
    }

    // 判定特定类型是否 serialization free
    template <typename T>
    bool is_serialization_free() const {
        // 条件 ①② 编译期已满足（register_local 的 static_assert）
        // 条件 ③ 运行时签名比较
        auto key = type_id<T>();
        auto local_it = local_signatures_.find(key);
        auto remote_it = remote_signatures_.find(key);

        if (local_it == local_signatures_.end()) return false;
        if (remote_it == remote_signatures_.end()) return false;

        return local_it->second == remote_it->second;  // 精确字符串匹配
    }

    // 获取诊断信息（签名不匹配时）
    template <typename T>
    std::string diagnose() const {
        auto key = type_id<T>();
        std::string result;
        result += "Type: " + key + "\n";
        
        auto local_it = local_signatures_.find(key);
        auto remote_it = remote_signatures_.find(key);
        
        if (local_it != local_signatures_.end())
            result += "  local:  " + std::string(local_it->second) + "\n";
        else
            result += "  local:  (not registered)\n";

        if (remote_it != remote_signatures_.end())
            result += "  remote: " + remote_it->second + "\n";
        else
            result += "  remote: (not registered)\n";

        return result;
    }

private:
    std::unordered_map<std::string, std::string_view> local_signatures_;
    std::unordered_map<std::string, std::string> remote_signatures_;

    template <typename T>
    static std::string type_id() {
        // 使用编译器提供的类型名或用户注册的名称
        return std::string(typeid(T).name());
    }
};

} // namespace boost::typelayout
```

### 5.3 握手协议

```text
═══════════════════════════════════════════════════════════
  握手协议（Handshake Protocol）
═══════════════════════════════════════════════════════════

  时间线：
    ┌─────────────────┐                    ┌─────────────────┐
    │     端 A         │                    │     端 B         │
    └────────┬────────┘                    └────────┬────────┘
             │                                      │
    Phase 1: │ ── SIG_OFFER(type_list + sigs) ────> │
    交换签名  │ <── SIG_OFFER(type_list + sigs) ──  │
             │                                      │
    Phase 2: │  本地比较签名，生成兼容性表            │  本地比较签名
    判定      │  compatible_types = { T1, T3, T7 }   │  compatible_types = { T1, T3, T7 }
             │                                      │
    Phase 3: │ ══ 数据传输（memcpy 路径）═══════> │
    传输      │ <═══════════════════════════════════  │
             │                                      │

  SIG_OFFER 消息格式：

    ┌──────────┬──────────┬───────────────────────────────┐
    │ num_types│ type_name│ signature_string              │
    │ (u32)    │ (len+str)│ (len+str)                     │
    ├──────────┼──────────┼───────────────────────────────┤
    │ 3        │ "Point"  │ "[64-le]{i32@0,i32@4|8|4}"    │
    │          │ "Sensor" │ "[64-le]{c8@0,i32@4,f64@8|16|8}" │
    │          │ "Key256" │ "[64-le]O(AesKey256|32|1)"    │
    └──────────┴──────────┴───────────────────────────────┘

  要点：
    ● 签名字符串完整传输（不是哈希）
    ● 握手只发生一次（连接建立时）
    ● 之后数据传输路径上不再有签名比较
    ● 签名不匹配的类型回退到普通序列化路径
```

---

## 6. 使用流程

### 6.1 编译期部分

```cpp
// ── 步骤 1：定义类型 ──

struct Point {
    int32_t x;
    int32_t y;
};

struct Sensor {
    char     id;
    int32_t  value;
    double   timestamp;
};

struct AesKey256 { unsigned char data[32]; };
TYPELAYOUT_REGISTER_OPAQUE(AesKey256, "AesKey256", false)

// ── 步骤 2：编译期检查 ──

static_assert(is_local_serialization_free_v<Point>,
    "Point should be serialization-free locally");

static_assert(is_local_serialization_free_v<Sensor>,
    "Sensor should be serialization-free locally");

static_assert(is_local_serialization_free_v<AesKey256>,
    "AesKey256 should be serialization-free locally");

// 以下会编译失败（包含指针）：
// static_assert(is_local_serialization_free_v<std::string>);
```

### 6.2 运行时部分

```cpp
// ── 步骤 3：初始化注册表 ──

boost::typelayout::SignatureRegistry registry;
registry.register_local<Point>();
registry.register_local<Sensor>();
registry.register_local<AesKey256>();

// ── 步骤 4：握手 ──

// 导出本端签名 → 发送给远端
auto local_sigs = registry.export_signatures();
send(peer, local_sigs);

// 接收远端签名 → 导入
auto remote_sigs = recv(peer);
registry.import_remote_signatures(remote_sigs);

// ── 步骤 5：判定并传输 ──

template <typename T>
void send_object(const T& obj, Connection& conn,
                 const SignatureRegistry& reg) {
    if constexpr (!is_local_serialization_free_v<T>) {
        // 编译期已知不可能 serialization free → 序列化路径
        auto buf = serialize(obj);
        conn.send(SERIALIZED, buf);
    } else {
        // 本端条件满足，检查远端
        if (reg.is_serialization_free<T>()) {
            // 签名匹配 → memcpy 路径
            conn.send(MEMCPY, &obj, sizeof(T));
        } else {
            // 签名不匹配 → 序列化回退
            auto buf = serialize(obj);
            conn.send(SERIALIZED, buf);
        }
    }
}
```

---

## 7. 诊断与错误处理

### 7.1 编译期诊断

```cpp
// 明确的 static_assert 消息

// 情况 1：非 trivially_copyable
struct Bad1 {
    std::string name;
    int value;
};
// serialization_free_assert<Bad1>();
// → error: "Serialization-free requires trivially_copyable.
//           Type has non-trivial copy/move/destructor."

// 情况 2：包含指针
struct Bad2 {
    int* data;
    int  size;
};
// serialization_free_assert<Bad2>();
// → error: "Serialization-free requires no pointers.
//           Type contains pointer or reference members
//           that are address-space dependent."
```

### 7.2 运行时诊断

```text
签名不匹配时的诊断输出（直接对比签名字符串）：

  Type: Sensor
    local:  [64-le]{c8@0,i32@4,f64@8|16|8}
    remote: [32-le]{c8@0,i32@4,f64@8|16|4}
    
  差异分析：
    位 1-2: "64" vs "32"         → 指针宽度不同
    位 4-5: "le" vs "le"         → 字节序相同
    字段签名: 相同
    total_size: 16 vs 16         → 相同
    alignment: 8 vs 4            → 不同（64位默认8对齐，32位默认4对齐）
    
  结论：平台差异导致 alignment 不同 → 不能 serialization free
        → 需要使用序列化路径

签名字符串本身就是诊断信息，不需要额外元数据。
```

---

## 8. 安全分类与签名的关系

```text
五级安全分类（基于签名内容推导）：

  Level 0: UNSAFE
    trivially_copyable = false
    → 不在签名系统中（编译期直接拒绝）

  Level 1: LOCAL_ONLY
    trivially_copyable = true, has_pointer = true
    → 签名可计算，但 has_pointer 阻止 serialization free
    → 仅可用于进程内 memcpy（同地址空间）

  Level 2: PLATFORM_DEPENDENT
    trivially_copyable = true, has_pointer = false
    签名包含 platform-variant 类型（long, size_t, time_t...）
    → 同平台 serialization free 可行
    → 跨平台需要签名匹配确认

  Level 3: PORTABLE
    trivially_copyable = true, has_pointer = false
    签名仅包含固定宽度类型（int32_t, float, double...）
    → 同字节序平台几乎必然签名匹配
    → 仍需握手确认（严谨性）

  Level 4: UNIVERSAL
    Level 3 + 单字节类型或字节序无关
    → 任意平台签名必然匹配

判定方式：
  Level 0-1: 编译期确定，不需要签名
  Level 2-4: 编译期分类，但最终兼容性由签名匹配决定
```

---

## 9. 设计约束与边界

### 9.1 明确支持

```text
✓ 基本类型（int, float, char, bool, enum...）
✓ 固定宽度类型（int32_t, uint64_t...）
✓ 平台相关类型（long, size_t, ptrdiff_t...）
✓ POD 结构体（含嵌套）
✓ 固定大小数组（T[N]）
✓ std::array<T, N>（如果 trivially_copyable）
✓ union（含变体类型签名）
✓ enum / enum class
✓ opaque 类型（通过 TYPELAYOUT_REGISTER_OPAQUE）
✓ 带 padding 的结构体（padding 不关心内容）
✓ 带 alignas 的结构体
✓ 嵌套组合（struct 包含 struct/array/enum/opaque）
```

### 9.2 明确不支持

```text
✗ 包含指针/引用的类型
    → has_pointer = true → Level 1 → 不可 serialization free

✗ 非 trivially_copyable 类型
    → std::string, std::vector, std::shared_ptr...
    → 编译期 static_assert 拒绝

✗ 虚函数表（vtable 指针）
    → 多态类型不是 trivially_copyable → 自动排除

✗ 位域（bit-field）
    → 签名系统当前不编码位域布局
    → 可作为 opaque 类型处理（用户断言安全性）

✗ 动态大小类型（flexible array member, VLA）
    → sizeof 不确定 → 无法生成签名
```

### 9.3 用户责任

```text
对于 opaque 类型，用户承担以下责任：

  1. Tag 唯一性：同一程序中不同 opaque 类型的 Tag 不能重复
  2. has_pointer 准确性：错误声明可能导致跨进程传输指针 → 崩溃
  3. 版本一致性：两端的 opaque 类型定义必须布局兼容
  4. trivially_copyable 真实性：编译器可检查，但用户应理解含义

  TypeLayout 对 opaque 类型不做内部布局验证。
  签名中 "O(Tag|size|align)" 的安全性完全依赖用户断言。
```

---

## 10. 实现要点总结

```text
核心变更（相对于使用哈希的版本）：

  1. 删除 detail/hash.hpp 在 serialization-free 判定中的使用
     → hash.hpp 可保留用于其他用途（如类型索引），但不参与兼容性判定

  2. layout_traits<T> 的签名字段类型
     → 从 uint64_t signature_hash 改为 Fixed