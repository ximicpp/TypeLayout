# TypeLayout & Serialization Free 方案文档

## 1. 概述

### 1.1 目标

提供编译期类型布局内省机制（TypeLayout），在此基础上实现零开销的
serialization free 传输判定，支持普通类型和 opaque 类型。

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
    ③ && signature_match(local, remote)   // 两端内存布局完全一致

③ 自动覆盖：
  - padding 差异（offset 不同 → 签名不同）
  - platform variant（long/size_t 宽度不同 → 签名不同）
  - 字节序差异（签名编码字节序）
  - alignment 差异（alignof 不同 → 签名不同）
```

---

## 2. TypeLayout 核心层

### 2.1 layout_traits 主模板

```cpp
template <typename T, typename Enable = void>
struct layout_traits {
    // 未特化 → 编译失败，强制用户注册类型
    static_assert(sizeof(T) == 0, 
        "Type not registered with TypeLayout. "
        "Use TYPELAYOUT_REGISTER or TYPELAYOUT_REGISTER_OPAQUE.");
};
```

### 2.2 普通类型注册（字段可见）

```cpp
// 用户通过宏注册普通类型
#define TYPELAYOUT_REGISTER(Type, ...)                              \
template <>                                                         \
struct layout_traits<Type> {                                        \
    static constexpr bool is_opaque = false;                        \
    static constexpr size_t total_size = sizeof(Type);              \
    static constexpr size_t alignment = alignof(Type);              \
    static constexpr bool has_pointer = /* 由字段推导 */;            \
    static constexpr auto fields = make_field_list(__VA_ARGS__);    \
    static constexpr auto signature = compute_signature(            \
        fields, total_size, alignment, endianness());               \
};

// 使用示例
struct Packet {
    uint32_t seq;
    float    x;
    float    y;
    float    z;
};

TYPELAYOUT_REGISTER(Packet,
    FIELD(Packet, seq),
    FIELD(Packet, x),
    FIELD(Packet, y),
    FIELD(Packet, z)
)
```

### 2.3 核心层提供的编译期信息

```text
layout_traits<T>::is_opaque             → bool，是否为 opaque 类型
layout_traits<T>::total_size            → sizeof(T)
layout_traits<T>::alignment             → alignof(T)
layout_traits<T>::has_pointer           → 是否含指针字段
layout_traits<T>::fields                → 字段列表（opaque 时为空）
layout_traits<T>::signature             → 类型布局签名（uint64/uint128 哈希）
```

### 2.4 签名计算

```cpp
// 签名编码以下信息：
struct signature_input {
    // — 所有类型共有 —
    size_t      total_size;
    size_t      alignment;
    endian_t    byte_order;     // little / big
    bool        is_opaque;
    
    // — 普通类型 —
    struct field_info {
        size_t  offset;
        size_t  size;
        kind_t  kind;           // i8/u8/i16/u16/i32/u32/i64/u64/f32/f64/...
    };
    field_info  fields[N];      // 按 offset 排序
    
    // — opaque 类型 —
    uint64_t    opaque_type_id; // 用户提供的类型标识符
};

// 签名 = hash(signature_input 的确定性序列化)
// 使用 FNV-1a 或 xxHash 等确定性哈希
static constexpr uint64_t signature = 
    constexpr_hash(total_size, alignment, byte_order, is_opaque, fields_or_type_id);
```

---

## 3. Opaque 类型支持

### 3.1 问题分析

```text
Opaque 类型的挑战：

  ┌──────────────────────────┬────────────┬────────────┐
  │ 属性                      │ 普通类型    │ Opaque 类型 │
  ├──────────────────────────┼────────────┼────────────┤
  │ sizeof 已知               │ ✅ 自动     │ ✅ 自动     │
  │ alignof 已知              │ ✅ 自动     │ ✅ 自动     │
  │ 字段列表                  │ ✅ 用户注册  │ ❌ 不可见   │
  │ trivially_copyable        │ ✅ 编译器   │ ❓ 需断言   │
  │ has_pointer               │ ✅ 字段推导  │ ❓ 需断言   │
  │ 签名内容                  │ 字段编码    │ 无字段可编码 │
  └──────────────────────────┴────────────┴────────────┘

  核心问题：没有字段信息，如何生成有意义的签名？
```

### 3.2 解决方案：用户断言 + 类型标识符

```text
设计原则：

  1. 用户必须显式断言 opaque 类型的安全属性
  2. 用户必须提供一个全局唯一的 type_id
  3. TypeLayout 基于 (size, align, endian, type_id) 生成签名
  4. 两端的 opaque 类型只有 type_id 完全匹配时，签名才一致
  5. type_id 由用户管理（可以是字符串哈希、UUID、版本号等）
```

### 3.3 Opaque 注册宏

```cpp
// ============================================================
// TYPELAYOUT_REGISTER_OPAQUE
//
// 参数：
//   Type          — 类型名
//   type_id       — uint64_t，全局唯一标识符
//   pointer_free  — bool，用户断言：是否不含指针
// ============================================================

#define TYPELAYOUT_REGISTER_OPAQUE(Type, type_id, pointer_free)     \
template <>                                                         \
struct layout_traits<Type> {                                        \
    static constexpr bool   is_opaque = true;                       \
    static constexpr size_t total_size = sizeof(Type);              \
    static constexpr size_t alignment = alignof(Type);              \
    static constexpr bool   has_pointer = !(pointer_free);          \
    static constexpr uint64_t opaque_type_id = (type_id);           \
                                                                    \
    /* opaque 类型无字段列表 */                                       \
    static constexpr auto fields = empty_field_list{};              \
                                                                    \
    /* 签名基于 size + align + endian + opaque_flag + type_id */    \
    static constexpr auto signature = compute_opaque_signature(     \
        total_size, alignment, endianness(),                        \
        opaque_type_id);                                            \
                                                                    \
    /* 编译期静态断言：opaque 类型必须 trivially_copyable */           \
    static_assert(std::is_trivially_copyable_v<Type>,               \
        "Opaque type must be trivially copyable for "               \
        "serialization free support");                              \
};
```

### 3.4 使用示例

```cpp
// ---- 场景 1：FFI 类型 ----

// 来自 C 库的不透明句柄
extern "C" {
    struct sensor_reading_t {
        char opaque_data[64];  // 内部结构未知，但已知无指针
    };
}

// 注册为 opaque 类型
TYPELAYOUT_REGISTER_OPAQUE(
    sensor_reading_t,
    0xA1B2C3D4E5F60001ULL,   // 由 C 库文档定义的类型 ID
    true                      // 用户断言：不含指针
)


// ---- 场景 2：加密哈希摘要 ----

struct sha256_digest {
    uint8_t bytes[32];
};

// 虽然可以用普通注册（字段是 uint8_t[32]），
// 但作为 opaque 更简洁——我们不关心内部字段
TYPELAYOUT_REGISTER_OPAQUE(
    sha256_digest,
    0x00000000SHA256DDULL,
    true
)


// ---- 场景 3：包含 opaque 字段的复合类型 ----

struct SensorPacket {
    uint32_t          timestamp;
    sensor_reading_t  reading;     // opaque 字段
    sha256_digest     checksum;    // opaque 字段
};

TYPELAYOUT_REGISTER(SensorPacket,
    FIELD(SensorPacket, timestamp),
    OPAQUE_FIELD(SensorPacket, reading),   // 标记为 opaque 字段
    OPAQUE_FIELD(SensorPacket, checksum)   // 标记为 opaque 字段
)
```

### 3.5 复合类型中 opaque 字段的处理

```text
当普通类型包含 opaque 字段时：

  has_pointer 推导规则：
    has_pointer(SensorPacket) = 
        has_pointer(uint32_t)          → false
        || has_pointer(sensor_reading_t) → false（用户断言）
        || has_pointer(sha256_digest)    → false（用户断言）
        = false

  签名计算：
    对 opaque 字段，签名不展开其内部字段，
    而是嵌入该字段的 opaque_signature：

    signature(SensorPacket) = hash(
        total_size = sizeof(SensorPacket),
        alignment  = alignof(SensorPacket),
        endian     = little,
        field[0]   = { offset=0,  size=4,  kind=u32 },
        field[1]   = { offset=4,  size=64, kind=opaque, 
                       sub_sig=signature(sensor_reading_t) },
        field[2]   = { offset=68, size=32, kind=opaque, 
                       sub_sig=signature(sha256_digest) },
    )

  这样：
    如果 sensor_reading_t 的 type_id 或 size 在对端不同 → 子签名不同
    → SensorPacket 的签名不同 → 判定不能 serialization free
```

---

## 4. Serialization Free Trait 层

### 4.1 编译期 Trait（单端判定）

```cpp
// ============================================================
// is_local_serialization_free<T>
//
// 编译期判定：T 在本地是否满足 serialization free 的前提条件
// 不涉及远端签名比较（那是运行时的事）
// ============================================================

template <typename T>
struct is_local_serialization_free {
    static constexpr bool value =
        std::is_trivially_copyable_v<T>
        && !layout_traits<T>::has_pointer;
};

template <typename T>
inline constexpr bool is_local_serialization_free_v =
    is_local_serialization_free<T>::value;
```

### 4.2 运行时判定（双端签名比较）

```cpp
// ============================================================
// 运行时：连接建立时交换签名
// ============================================================

template <typename T>
bool is_serialization_free(uint64_t remote_signature) {
    return is_local_serialization_free_v<T>
        && (layout_traits<T>::signature == remote_signature);
}
```

### 4.3 编译期断言

```cpp
// ============================================================
// 编译期断言：静态保证某类型可 serialization free
// 用于同构系统（已知两端架构相同）
// ============================================================

template <typename T>
struct serialization_free_assert {
    static_assert(std::is_trivially_copyable_v<T>,
        "serialization free requires trivially copyable");
    static_assert(!layout_traits<T>::has_pointer,
        "serialization free requires no pointer fields");
    // 签名比较在同构系统中不需要运行时检查
    // 但 has_platform_variant 作为额外保险
    static constexpr bool value = true;
};
```

### 4.4 对 Opaque 类型的 Trait 行为

```text
Opaque 类型走完全相同的判定路径：

  is_local_serialization_free<sensor_reading_t>
    = trivially_copyable(sensor_reading_t)    // static_assert 在注册时已验证
      && !has_pointer(sensor_reading_t)        // 用户断言 pointer_free=true
    = true

  is_serialization_free<sensor_reading_t>(remote_sig)
    = is_local_serialization_free_v<sensor_reading_t>
      && (signature(sensor_reading_t) == remote_sig)
    = true && (local_sig == remote_sig)

  判定逻辑完全统一，opaque 和非 opaque 无分支。
```

---

## 5. 签名交换协议

### 5.1 连接建立时

```text
┌──────────────┐                     ┌──────────────┐
│   端 A        │                     │   端 B        │
│              │                     │              │
│  编译期已知：  │                     │  编译期已知：  │
│  sig(Packet) │                     │  sig(Packet) │
│  sig(Event)  │                     │  sig(Event)  │
│              │                     │              │
│  ① 发送签名表 ├────────────────────►│              │
│              │                     │  ② 比较签名   │
│              │◄────────────────────┤  ③ 回复结果   │
│              │                     │              │
│  ④ 确认哪些类 │                     │              │
│    型可以      │                     │              │
│    memcpy     │                     │              │
└──────────────┘                     └──────────────┘
```

### 5.2 签名表格式

```cpp
struct signature_entry {
    uint64_t type_id;      // 类型标识（名字哈希或用户指定）
    uint64_t signature;    // 布局签名
    uint32_t total_size;   // sizeof(T)
};

struct signature_table {
    uint32_t         count;
    signature_entry  entries[];
};
```

### 5.3 判定结果缓存

```cpp
// 连接建立后，缓存判定结果
template <typename T>
struct serialization_decision {
    enum mode_t { MEMCPY, SERIALIZE, INCOMPATIBLE };
    static mode_t mode;  // 连接建立时设定，此后不变
};

// 发送时
template <typename T>
void send(const T& obj) {
    if constexpr (!is_local_serialization_free_v<T>) {
        // 编译期已知不可能 memcpy → 直接走序列化
        serialize_and_send(obj);
    } else {
        // 编译期可能 memcpy → 运行时查缓存
        if (serialization_decision<T>::mode == MEMCPY) {
            raw_send(&obj, sizeof(T));     // serialization free!
        } else {
            serialize_and_send(obj);       // fallback
        }
    }
}
```

---

## 6. TypeLayout 核心层修改清单

### 6.1 新增内容

```text
┌────┬──────────────────────────────┬────────────────────────────────┐
│ #  │ 修改项                        │ 说明                           │
├────┼──────────────────────────────┼────────────────────────────────┤
│ 1  │ is_opaque 标志                │ layout_traits 新增 bool 字段    │
│ 2  │ opaque_type_id               │ opaque 类型的全局唯一标识符      │
│ 3  │ TYPELAYOUT_REGISTER_OPAQUE 宏 │ opaque 类型注册入口             │
│ 4  │ OPAQUE_FIELD 宏               │ 普通类型中的 opaque 字段标记     │
│ 5  │ compute_opaque_signature     │ 基于 type_id 的签名计算函数      │
│ 6  │ kind_t 新增 OPAQUE 种类       │ 字段种类枚举新增 opaque 类别     │
│ 7  │ has_pointer 对 opaque 的推导   │ 取用户断言值，而非字段推导       │
└────┴──────────────────────────────┴────────────────────────────────┘
```

### 6.2 不修改的内容

```text
  ● layout_traits 的基本结构（total_size, alignment, fields, signature）
  ● 普通类型的注册方式（TYPELAYOUT_REGISTER）
  ● 签名比较的接口和语义
  ● has_padding / has_platform_variant（诊断信息，保持原样）

  原则：opaque 是扩展，不改变已有非 opaque 类型的行为。
```

### 6.3 字段种类枚举扩展

```cpp
enum class kind_t : uint8_t {
    // 基本类型
    I8, U8, I16, U16, I32, U32, I64, U64,
    F32, F64,
    
    // 复合类型
    STRUCT,         // 内嵌已注册的普通 struct
    ARRAY,          // 固定长度数组
    
    // --- 新增 ---
    OPAQUE,         // 内嵌的 opaque 类型
                    // 字段描述中携带 sub_signature 而非展开字段
};
```

### 6.4 字段描述扩展

```cpp
struct field_descriptor {
    size_t    offset;
    size_t    size;
    kind_t    kind;
    
    // 仅当 kind == STRUCT 时有效
    uint64_t  sub_signature;     // 已有：嵌套 struct 的签名
    
    // 仅当 kind == OPAQUE 时有效（复用 sub_signature 字段）
    // sub_signature = opaque 类型的签名（含 type_id）
};
```

---

## 7. 安全性考量

### 7.1 Opaque 断言的风险

```text
用户断言 pointer_free=true，但实际含指针 → 灾难性后果

  缓解措施：
    ● 文档明确：断言错误 = UB，用户负全责
    ● 提供运行时可选验证（debug 模式）：
      - 扫描 opaque 区域的字节，检查是否像指针
      - 不保证检测率，但可作为辅助
    ● 命名强调危险性：
      TYPELAYOUT_REGISTER_OPAQUE_UNSAFE(...)  // 可选的更醒目名字
```

### 7.2 Type ID 冲突

```text
两个不同的 opaque 类型使用了相同的 type_id → 签名碰撞

  缓解措施：
    ● type_id 建议使用编译期字符串哈希：
      constexpr uint64_t id = typelayout::hash("mylib::sensor_reading_t/v2");
    ● 或使用 __FILE__ + __LINE__ 自动生成
    ● CI 中可静态检查全项目的 type_id 唯一性
```

### 7.3 版本演进

```text
Opaque 类型的内部布局变了，但 type_id 没更新 → 静默错误

  缓解措施：
    ● 将版本号编入 type_id：
      TYPELAYOUT_REGISTER_OPAQUE(sensor_reading_t, 
          OPAQUE_ID("sensor_reading_t", 2),  // version 2
          true)
    ● 如果 size 变了 → 签名自动变（size 参与签名计算）
    ● 如果 size 没变但内部布局变了 → 只能靠 type_id 版本号区分
```

---

## 8. 完整示例

```cpp
#include <typelayout/core.h>
#include <typelayout/serialization_free.h>

// ============================================================
// 1. 定义类型
// ============================================================

// 普通类型
struct Vec3 {
    float x, y, z;
};

// Opaque 类型（来自外部 C 库）
extern "C" {
    struct imu_raw_t {
        char data[48];
    };
}

// 包含 opaque 字段的复合类型
struct SensorFrame {
    uint64_t  timestamp;
    Vec3      position;
    Vec3      velocity;
    imu_raw_t imu;           // opaque
};

// ============================================================
// 2. 注册类型
// ============================================================

TYPELAYOUT_REGISTER(Vec3,
    FIELD(Vec3, x),
    FIELD(Vec3, y),
    FIELD(Vec3, z)
)

TYPELAYOUT_REGISTER_OPAQUE(imu_raw_t,
    OPAQUE_ID("vendor::imu_raw_t", 1),    // 类型 ID + 版本
    true                                   // 断言：无指针
)

TYPELAYOUT_REGISTER(SensorFrame,
    FIELD(SensorFrame, timestamp),
    FIELD(SensorFrame, position),
    FIELD(SensorFrame, velocity),
    OPAQUE_FIELD(SensorFrame, imu)
)

// ============================================================
// 3. 编译期检查
// ============================================================

static_assert(is_local_serialization_free_v<Vec3>);
static_assert(is_local_serialization_free_v<imu_raw_t>);
static_assert(is_local_serialization_free_v<SensorFrame>);

// ============================================================
// 4. 运行时使用
// ============================================================

void on_connection_established(Connection& conn) {
    // 交换签名
    auto remote_sigs = conn.exchange_signatures({
        { type_id_of<SensorFrame>(), 
          layout_traits<SensorFrame>::signature,
          sizeof(SensorFrame) }
    });
    
    // 判定
    if (is_serialization_free<SensorFrame>(
            remote_sigs["SensorFrame"])) {
        conn.set_mode<SensorFrame>(MEMCPY);
    } else {
        conn.set_mode<SensorFrame>(SERIALIZE);
    }
}

void send_frame(Connection& conn, const SensorFrame& frame) {
    if (conn.mode<SensorFrame>() == MEMCPY) {
        conn.raw_send(&frame, sizeof(frame));    // serialization free!
    } else {
        conn.serialized_send(frame);             // fallback
    }
}
```

---

## 9. 总结

```text
┌─────────────────────────────────────────────────────────────┐
│                  Serialization Free 判定                      │
│                                                             │
│  serialization_free(T) =                                    │
│      trivially_copyable(T)          // 语言层                │
│      && !has_pointer(T)             // 语义层                │
│      && signature_match(A, B)       // 布局层                │
│                                                             │
│  三个条件，统一适用于普通类型和 opaque 类型。                    │
│                                                             │
│  普通类型：自动推导 has_pointer，自动计算签名                    │
│  Opaque 类型：用户断言 has_pointer，type_id 参与签名            │
│                                                             │
│  TypeLayout 核心层修改：                                      │
│    + is_opaque 标志                                          │
│    + opaque_type_id                                          │
│    + TYPELAYOUT_REGISTER_OPAQUE 宏                           │
│    + OPAQUE_FIELD 宏                                         │
│    + kind_t::OPAQUE 种类                                     │
│    + compute_opaque_signature 函数                            │
│                                                             │
│  不改变已有非 opaque 类型的任何行为。                            │
└─────────────────────────────────────────────────────────────┘
```