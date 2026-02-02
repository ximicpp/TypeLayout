## ADDED Requirements

### Requirement: 序列化功能缺口分析报告

TypeLayout 序列化兼容性功能 SHALL 有完整的缺口分析文档，指导后续功能演进。

#### Scenario: 缺口清单已识别
- **GIVEN** 当前 TypeLayout 1.0 实现
- **WHEN** 从工业序列化场景审视功能
- **THEN** 已识别以下功能缺口：

---

## 功能缺口详细分析

### 缺口 1: 版本演进兼容性 (Version Evolution)

**问题描述**:
当前 TypeLayout 采用"签名完全匹配"模型，任何结构变更都会导致签名不同。
但实际工业场景中，需要支持向后兼容的版本演进：
- 添加新字段（尾部追加）
- 废弃字段（保留占位）
- 字段重命名（逻辑无关）

**当前状态**: ❌ 不支持

**潜在解决方案**:
```cpp
// 方案 A: 版本号嵌入签名
template <typename T, uint32_t Version>
constexpr auto get_versioned_signature();

// 方案 B: 兼容性比较函数
template <typename Old, typename New>
consteval CompatibilityResult check_backward_compatible();
// 返回: Compatible, IncompatibleAddition, IncompatibleRemoval, etc.
```

**优先级**: ⭐⭐⭐ 高
**复杂度**: 中等
**是否需要实现**: 可选 - 高级用户场景

---

### 缺口 2: 字节序转换 (Endianness Conversion)

**问题描述**:
`PlatformSet` 可以检测大小端不匹配，但不提供自动转换能力。
跨平台（如 ARM big-endian 与 x86）通信时，用户需要自行处理字节序。

**当前状态**: ⚠️ 检测但不转换

**潜在解决方案**:
```cpp
// 方案: 提供字节序转换工具函数
template <typename T>
constexpr T to_big_endian(const T& value);

template <typename T>
constexpr T from_big_endian(const T& value);

// 或 network byte order 标准函数
template <typename T>
constexpr T to_network_order(const T& value);
```

**优先级**: ⭐⭐ 中
**复杂度**: 低
**是否需要实现**: 可选 - 超出核心范围，可引导用户使用 Boost.Endian

---

### 缺口 3: 运行时签名验证 (Runtime Signature Validation)

**问题描述**:
当前签名是编译时字符串，缺少标准的运行时验证协议。
实际场景中需要：
- 将签名嵌入二进制数据头部
- 从网络数据包读取签名并验证
- 文件格式中的签名魔数

**当前状态**: ❌ 无标准协议

**潜在解决方案**:
```cpp
// 方案 A: 签名头结构
struct SignatureHeader {
    uint32_t magic;           // 0x544C5347 "TLSG"
    uint32_t hash;            // 快速比较
    uint16_t signature_len;   // 签名字符串长度
    // followed by: char signature[signature_len]
};

// 方案 B: 流式 API
template <typename T>
void write_signature_header(std::ostream& os);

template <typename T>
bool verify_signature_header(std::istream& is);
```

**优先级**: ⭐⭐⭐ 高
**复杂度**: 中等
**是否需要实现**: 推荐 - 实用性高

---

### 缺口 4: 签名差异报告 (Signature Diff Report)

**问题描述**:
当签名不匹配时，用户只知道"不兼容"，但不知道具体哪里不同。
调试跨进程/跨版本兼容性问题时非常困难。

**当前状态**: ❌ 无差异报告

**潜在解决方案**:
```cpp
// 方案: 结构化差异分析
struct SignatureDiff {
    bool compatible;
    std::vector<std::string> added_members;
    std::vector<std::string> removed_members;
    std::vector<std::pair<std::string, std::string>> changed_members;
    std::vector<std::string> offset_changes;
};

template <typename T1, typename T2>
consteval SignatureDiff compare_signatures();

// 运行时版本（从字符串比较）
SignatureDiff compare_signature_strings(
    std::string_view sig1, 
    std::string_view sig2
);
```

**优先级**: ⭐⭐ 中
**复杂度**: 高（需要签名解析器）
**是否需要实现**: 可选 - 调试辅助功能

---

### 缺口 5: 对齐填充初始化 (Padding Initialization)

**问题描述**:
结构体中的填充字节（padding bytes）是未定义的。
序列化时，填充字节可能包含随机数据，导致：
- 相同逻辑值产生不同的序列化结果
- 敏感信息泄漏（栈上残留数据）
- 签名哈希不稳定

**当前状态**: ⚠️ 未处理

**潜在解决方案**:
```cpp
// 方案 A: 零初始化包装器
template <Serializable T>
struct zero_padded {
    alignas(T) std::byte storage[sizeof(T)];
    
    zero_padded() { std::memset(storage, 0, sizeof(T)); }
    // ... accessor methods
};

// 方案 B: 安全序列化函数
template <Serializable T>
std::array<std::byte, sizeof(T)> safe_serialize(const T& value) {
    std::array<std::byte, sizeof(T)> result{};  // 零初始化
    std::memcpy(result.data(), &value, sizeof(T));
    return result;
}

// 方案 C: 编译时填充位置信息
template <typename T>
consteval auto get_padding_info();
// 返回填充字节的偏移量和大小列表
```

**优先级**: ⭐⭐⭐ 高
**复杂度**: 中等
**是否需要实现**: 推荐 - 安全关键

---

### 缺口 6: 自定义序列化钩子 (Customization Points)

**问题描述**:
某些类型虽然不满足 `is_serializable_v`（如含指针），但用户可能有自定义序列化逻辑。
当前库不提供自定义点让用户声明"这个类型我负责序列化"。

**当前状态**: ❌ 无自定义点

**潜在解决方案**:
```cpp
// 方案 A: 特化接口
template <typename T>
struct custom_serialization {
    static constexpr bool enabled = false;
    // 用户特化:
    // static constexpr bool enabled = true;
    // static void serialize(const T&, std::byte*);
    // static T deserialize(const std::byte*);
};

// 方案 B: 标记 trait
template <typename T>
struct is_custom_serializable : std::false_type {};

// 用户特化: 
// template<> struct is_custom_serializable<MyType> : std::true_type {};
```

**优先级**: ⭐ 低
**复杂度**: 低
**是否需要实现**: 可选 - 超出核心范围

---

### 缺口 7: 部分类型签名 (Partial Signature)

**问题描述**:
某些场景只关心部分字段的兼容性（如网络协议中的固定头部），
但当前只能获取完整类型签名。

**当前状态**: ❌ 不支持

**潜在解决方案**:
```cpp
// 方案: 成员子集签名
template <typename T, auto... Members>
consteval auto get_partial_signature();

// 使用示例
struct Packet {
    uint32_t magic;
    uint32_t version;
    uint32_t payload_size;
    std::array<char, 256> payload;
};

// 只检查头部兼容性
constexpr auto header_sig = get_partial_signature<Packet, 
    &Packet::magic, &Packet::version, &Packet::payload_size>();
```

**优先级**: ⭐ 低
**复杂度**: 高
**是否需要实现**: 可选 - 高级用户场景

---

## 优先级总结

| 缺口 | 优先级 | 建议 |
|------|--------|------|
| 运行时签名验证 | ⭐⭐⭐ | 推荐实现 |
| 对齐填充初始化 | ⭐⭐⭐ | 推荐实现 |
| 版本演进兼容性 | ⭐⭐⭐ | 可选实现 |
| 签名差异报告 | ⭐⭐ | 可选实现 |
| 字节序转换 | ⭐⭐ | 引导用户使用 Boost.Endian |
| 自定义序列化钩子 | ⭐ | 超出核心范围 |
| 部分类型签名 | ⭐ | 高级场景，推迟 |

---

#### Scenario: 分析报告用于指导开发
- **GIVEN** 本分析报告
- **WHEN** 规划 TypeLayout 1.1 或 2.0 功能
- **THEN** 应优先考虑:
  1. 运行时签名验证（实用性最高）
  2. 对齐填充处理（安全关键）
  3. 版本演进兼容性（工业场景需求）
