# Application Scenario Analysis: Layout Signature System

本文档系统分析 Boost.TypeLayout Layout 签名系统在典型应用场景中的
具体角色、形式化正确性保证和边界条件。

**核心符号速查：**
- ⟦T⟧_L — Layout 签名（字节身份，展平，无名称）
- L_P(T) — 字节布局元组 (sizeof, alignof, poly, fields)
- T ≅_mem U — memcpy 兼容性 (L_P(T) = L_P(U))

---

## 目录

1. [IPC / 共享内存](#1-ipc--共享内存)
2. [网络协议验证](#2-网络协议验证)
3. [文件格式兼容](#3-文件格式兼容)
4. [插件 ABI 验证](#4-插件-abi-验证)
5. [序列化与 API 兼容](#5-序列化与-api-兼容)
6. [场景总览矩阵](#6-场景总览矩阵)

---

## 1. IPC / 共享内存

### 1.1 场景描述

两个进程 P_A、P_B 通过 mmap 或 POSIX 共享内存共享一个内存区域。P_A 写入
`struct SharedData` 的实例，P_B 直接以相同类型读取。数据**不经过任何序列化**
——纯粹的 memcpy / 指针强制转换。

```cpp
// Process A (writer)
auto* shm = static_cast<SharedData*>(mmap(...));
shm->timestamp = now();
shm->value = 42.0;

// Process B (reader) — different binary, possibly different compilation unit
auto* shm = static_cast<SharedData*>(mmap(...));
double v = shm->value;  // 安全吗？
```

### 1.2 Layout 签名的作用

IPC 关心的是字节身份：P_B 读到的每个字节含义是否与 P_A 写入时一致。
Layout 签名精确编码了这一信息——字段偏移、大小、对齐、多态标记。

```cpp
// 编译期验证：两侧共享结构体的字节布局完全一致
static_assert(layout_signatures_match<ProcessA::SharedData,
                                       ProcessB::SharedData>());
```

### 1.3 形式化保证链

```
layout_signatures_match<T_A, T_B>() == true
    ⟹ ⟦T_A⟧_L = ⟦T_B⟧_L                  [API 语义]
    ⟹ L_P(T_A) = L_P(T_B)                  [Encoding Faithfulness]
    ⟹ T_A ≅_mem T_B                         [memcpy-compatibility]
    ⟹ sizeof(T_A) = sizeof(T_B)
       ∧ alignof(T_A) = alignof(T_B)
       ∧ fields_P(T_A) = fields_P(T_B)
    ⟹ memcpy(shm, &obj_A, sizeof(T_A))
       可安全以 T_B 读取
```

**Soundness（零误报）**：签名匹配 ⟹ memcpy 兼容，没有例外。

### 1.4 边界条件

| 边界 | 行为 | 保证 |
|------|------|------|
| **跨平台指针大小不同** | 架构前缀不同 (`[64-le]` vs `[32-le]`)，签名不匹配 | ✅ 自动检测 |
| **跨平台字节序不同** | 架构前缀不同 (`[64-le]` vs `[64-be]`)，签名不匹配 | ✅ 自动检测 |
| **padding 差异** | 字段偏移不同，签名不匹配 | ✅ 自动检测 |
| **含指针的结构体** | 签名匹配，但指针值跨进程无意义 | ⚠️ 语义不安全（SignatureRegistry 可检测） |
| **含虚函数的类型** | vptr 字段编入签名，但 vtable 跨进程无效 | ⚠️ 运行时语义不安全 |

**重要限制：** Layout 签名保证字节布局一致，但不保证数据语义可用。
含指针或 vtable 的类型不应直接跨进程 memcpy，即使签名匹配。
TypeLayout 的职责是验证布局，不是验证数据内容。

### 1.5 与传统方法对比

| 方法 | 验证范围 | 自动性 | 完备性 |
|------|---------|--------|--------|
| `static_assert(sizeof(T) == N)` | 仅总大小 | 手动 | ❌ 不检查字段偏移 |
| `static_assert(offsetof(T, f) == M)` | 单个字段偏移 | 手动 | ❌ 须逐字段列出 |
| `#pragma pack` | 控制对齐 | 手动 | ❌ 不验证，只控制 |
| **TypeLayout Layout 签名** | 所有字段的偏移、大小、类型 | **自动** | ✅ 递归覆盖全部字段 |

---

## 2. 网络协议验证

### 2.1 场景描述

客户端和服务端通过 TCP/UDP 交换二进制报文。报文头（wire format）是固定大小的
C++ 结构体，直接 cast 到网络缓冲区或从中 cast 出来。

```cpp
struct PacketHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t payload_len;
    uint32_t checksum;
};

// Sender
PacketHeader hdr{MAGIC, 1, MSG_DATA, payload.size(), crc32(payload)};
send(sock, &hdr, sizeof(hdr), 0);

// Receiver
PacketHeader hdr;
recv(sock, &hdr, sizeof(hdr), 0);
```

### 2.2 Layout 签名的作用

网络协议关心的是"发送的字节序列能否被对方正确解析"。
Layout 签名精确编码了每个字段的偏移和类型，这正是 wire format 一致性的定义。

```cpp
static_assert(layout_signatures_match<ClientPacketHeader, ServerPacketHeader>(),
    "Wire format mismatch between client and server!");
```

**完备性优势：**

传统方法需要逐字段验证，容易遗漏：
```cpp
// 手动验证 — O(n) 维护成本，新增字段时必须手动更新
static_assert(sizeof(PacketHeader) == 16);
static_assert(offsetof(PacketHeader, magic) == 0);
// ... 每个字段都要写
```

TypeLayout 自动覆盖所有字段。添加新字段、修改类型、调整顺序——签名自动变化，
不匹配自动被检测到。

### 2.3 字节序与边界

TypeLayout 在架构前缀中编码字节序：

```
[64-le]record[s:16,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2],...}  // x86_64 LE
[64-be]record[s:16,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2],...}  // SPARC64 BE
```

字节序不同 ⟹ 签名不同 ⟹ 布局"不兼容"——这是正确行为。
TypeLayout 检测字节序差异但**不执行转换**（职责分离）：用户负责使用
`ntohl/htonl` 或约定网络字节序。

### 2.4 形式化保证

```
layout_signatures_match<ClientHdr, ServerHdr>() == true
    ⟹ ⟦ClientHdr⟧_L = ⟦ServerHdr⟧_L
    ⟹ L_P(ClientHdr) = L_P(ServerHdr)
    ⟹ 每个字段在相同偏移处，相同大小，相同类型
    ⟹ send() 的字节序列可被 recv() 正确解析
```

---

## 3. 文件格式兼容

### 3.1 场景描述

一个应用程序使用固定大小的文件头写入二进制文件（传感器日志、数据库文件、
自定义配置文件）。需要验证：
1. **跨平台兼容**：Linux 上写入的文件能在 macOS 上正确读取
2. **跨版本兼容**：v1 软件写入的文件能被 v2 软件正确读取

### 3.2 Layout 签名保证跨平台 fread 安全

```cpp
struct FileHeader {
    char     magic[4];      // @0: bytes[s:4,a:1]
    uint32_t version;       // @4: u32[s:4,a:4]
    uint64_t timestamp;     // @8: u64[s:8,a:8]
    uint32_t entry_count;   // @16: u32[s:4,a:4]
    uint32_t reserved;      // @20: u32[s:4,a:4]
};
```

**保证链：**

```
layout_signatures_match<LinuxFileHeader, MacOSFileHeader>() == true
    ⟹ L_P(LinuxHdr) = L_P(MacOSHdr)
    ⟹ LinuxHdr ≅_mem MacOSHdr
    ⟹ fwrite 在 Linux 上写入的字节可被 fread 在 macOS 上正确读取
```

注意：这要求两个平台的架构前缀相同（相同指针宽度和字节序），
否则签名不匹配会**正确地**拒绝兼容性判断。

### 3.3 跨版本兼容检测

如果结构体中间插入了新字段，Layout 签名会自动检测不兼容：

```cpp
struct FileHeader_v3 {
    char     magic[4];
    uint32_t version;
    uint32_t priority;     // 新增字段！偏移后移
    uint64_t timestamp;
    uint32_t entry_count;
    uint32_t reserved;
};

// layout_signatures_match<FileHeader_v1, FileHeader_v3>() == false
// 正确：v1 数据无法被 v3 结构体直接读取
```

**局限：字段重命名不被检测。** 若只是重命名字段（字节布局不变），
Layout 签名不变，视为兼容。如需检测字段重命名，需要用户通过代码审查
或命名规范保证语义一致性。

### 3.4 决策矩阵

| 情况 | Layout 签名 | 行动 |
|------|:-----------:|------|
| 两版本签名匹配 | ✅ | 字节兼容，可直接 fread |
| 签名不匹配（新增/删除/移动字段） | ❌ | 字节不兼容，需要格式迁移 |

---

## 4. 插件 ABI 验证

### 4.1 场景描述

宿主程序通过 `dlopen` (Linux) 或 `LoadLibrary` (Windows) 动态加载插件。
宿主和插件通过共享的结构体类型交换数据。

如果宿主和插件的结构体字节布局不同（例如不同编译器选项导致不同 padding），
数据传递会产生未定义行为。

### 4.2 Layout 签名验证 ABI 兼容性

```cpp
// Host
struct PluginConfig { uint32_t version; uint64_t flags; char name[32]; };
plugin->init(&config);

// Plugin (独立编译)
void init(const PluginConfig* cfg) {
    uint64_t f = cfg->flags;  // 依赖字节布局一致
}
```

**编译期验证（联合编译场景）：**

```cpp
static_assert(layout_signatures_match<Host::PluginConfig,
                                       Plugin::PluginConfig>());
```

**运行时验证（独立编译场景）：**

```cpp
// Plugin 导出签名字符串
extern "C" const char* get_config_layout_sig() {
    static constexpr auto sig = get_layout_signature<PluginConfig>();
    return sig.c_str();
}

// Host 在 dlopen 后验证
const char* plugin_sig = ((decltype(&get_config_layout_sig))
    dlsym(handle, "get_config_layout_sig"))();

if (!is_transfer_safe<PluginConfig>(plugin_sig)) {
    dlclose(handle);
    return Error::ABIMismatch;
}
```

### 4.3 检测范围与局限

Layout 签名检测**数据布局相关的 ABI 不兼容**（字段名、偏移、大小、继承结构变化），
但以下内容**不在检测范围内**：

| 变更类型 | Layout 签名 | 建议工具 |
|---------|:-----------:|---------|
| 字段偏移/大小变化 | ✅ 检测 | TypeLayout |
| 字段数量变化 | ✅ 检测 | TypeLayout |
| 字段重命名（布局不变） | ❌ 不检测 | 代码审查 |
| 成员函数签名变化 | ❌ 不检测 | abi-compliance-checker |
| 虚函数表内容变化 | ❌ 不检测 | abi-compliance-checker |
| 静态成员变化 | ❌ 不检测 | 代码审查 |

---

## 5. 序列化与 API 兼容

### 5.1 序列化场景

TypeLayout 的 Layout 签名保证**字节层面**的布局一致性（fwrite/fread 安全）。

对于基于字段名映射的序列化框架（JSON、MessagePack），Layout 签名保证底层字节
兼容性；字段名的语义一致性需由用户通过命名规范或代码审查保证。

```
序列化框架类型          TypeLayout 能保证什么         用户还需做什么
─────────────────────────────────────────────────────────────────
fwrite/fread           ✅ 字节级完全兼容              无
JSON (基于名称)         ✅ 底层字节布局一致            ⚠️ 字段名语义一致性
MessagePack (基于顺序)  ✅ 字段顺序/偏移未变           ⚠️ 字段语义一致性
Protocol Buffers       ❌ TypeLayout 不分析 .proto     用 protobuf 自身版本管理
```

**关键区分：** Layout 签名匹配意味着"可以安全地将内存中的字节解释为相同的结构体"，
不意味着"字段的业务语义没有发生变化"。字段重命名（`timeout_ms` → `timeout_seconds`）
在 Layout 层是不可见的，因为字节布局未变。

### 5.2 API 兼容场景

TypeLayout 验证**数据结构的字节级 ABI 兼容性**（字段偏移、大小、对齐）。
完整的 C++ API 兼容性检查还需要其他工具：

| 兼容性维度 | TypeLayout | 补充工具 |
|-----------|:---------:|---------|
| 字段偏移/大小（ABI 核心） | ✅ | — |
| 虚函数表兼容性 | ❌ | abi-compliance-checker |
| 成员函数签名 | ❌ | abi-compliance-checker |
| 模板实例化兼容性 | ❌ | 专用模板分析工具 |
| 符号级 ABI | ❌ | libabigail |

**定位：** TypeLayout 是数据结构 ABI 的**编译期守卫**，是现有 ABI 工具链的
**互补工具**，不是替代品。

---

## 6. 场景总览矩阵

### 6.1 场景与核心 API

| 场景 | 核心 API | 核心定理 |
|------|---------|---------|
| IPC / 共享内存 | `layout_signatures_match` | Soundness (Thm 4.8) |
| 网络协议验证 | `layout_signatures_match` | Encoding Faithfulness (Thm 4.6) |
| 文件格式兼容 | `layout_signatures_match` + `SigExporter` | Soundness (Thm 4.8) |
| 插件 ABI 验证 | `get_layout_signature` + `is_transfer_safe` | Soundness (Thm 4.8) |
| 跨平台兼容检查 | `SigExporter` → `.sig.hpp` → `CompatReporter` | Injectivity (Cor 4.7) |

### 6.2 Layout 签名的覆盖范围

TypeLayout Layout 签名编码了以下所有信息（**自动、完备、递归**）：

| 信息 | Layout 签名 |
|------|:-----------:|
| sizeof(T) | ✅ `s:N` |
| alignof(T) | ✅ `a:N` |
| 每个字段的字节偏移 | ✅ `@OFF:` |
| 每个字段的大小和对齐 | ✅ `[s:N,a:M]` |
| 字段类型（递归） | ✅ 类型符号 |
| 填充字节（隐含于偏移间隙） | ✅ 隐式 |
| 虚函数表指针 | ✅ 合成 `ptr[s:N,a:N]` |
| 架构（指针宽度、字节序） | ✅ 架构前缀 |
| 位域（byte.bit 精度） | ✅ `@B.b:bits<W,T>` |
| **字段名称** | ❌ 不编码（设计如此） |
| **继承层次结构** | ❌ 展平 |
| **成员函数** | ❌ 不反射 |

### 6.3 全局正确性

**定理（场景安全性）。** 对于所有上述场景，如果 Layout 签名匹配，
则该场景的字节级安全条件成立：

```
layout_signatures_match(T, U) == true
⟹ L_P(T) = L_P(U)     [Encoding Faithfulness]
⟹ T ≅_mem U            [Soundness]
⟹ 字节级数据交换安全
```

**无一场景存在字节级误报（false positive）。** 签名匹配总是意味着字节布局一致。

**完备性边界。** Layout 签名对**字节布局维度**完备，但对以下维度不完备（by design）：
- 字段名称语义（字段重命名不被检测）
- 成员函数/虚函数表内容
- 外部 IDL schema（protobuf field_number）
- 业务语义（`timeout_ms` vs `timeout_seconds` 的含义）

这些不完备项均为**有意的设计选择**：TypeLayout 回答"字节是否相同"，
不回答"含义是否相同"。后者需要代码审查、命名规范和外部工具。
