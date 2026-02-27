# Application Scenario Analysis: Two-Layer Signature System

本文档系统分析 Boost.TypeLayout 两层签名系统（Layout / Definition）在 6 个典型应用场景中的
具体角色、层选择理由、形式化正确性保证和边界条件。

**前置阅读：** 本文档假设读者熟悉 `PROOFS.md` 中的核心定理和术语。

**核心符号速查：**
- ⟦T⟧_L — Layout 签名（字节身份，展平，无名称）
- ⟦T⟧_D — Definition 签名（结构身份，保留层次和名称）
- L_P(T) — 字节布局元组 (sizeof, alignof, poly, fields)
- D_P(T) — 结构树元组 (sizeof, alignof, poly, bases, named_fields)
- T ≅_mem U — memcmp 兼容性 (L_P(T) = L_P(U))

---

## 目录

1. [IPC / 共享内存](#1-ipc--共享内存)
2. [网络协议验证](#2-网络协议验证)
3. [文件格式兼容](#3-文件格式兼容)
4. [插件 ABI / ODR 检测](#4-插件-abi--odr-检测)
5. [序列化版本检查](#5-序列化版本检查)
6. [API 兼容检查](#6-api-兼容检查)
7. [场景总览矩阵](#7-场景总览矩阵)

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

### 1.2 推荐层：Layout（V1 为主）

**核心理由：IPC 关心的是字节身份，不关心名称或继承层次。**

| 关注点 | Layout (V1) | Definition (V2) |
|--------|:-----------:|:----------------:|
| 字段字节偏移 | ✅ 编码 | ✅ 编码 |
| 字段大小/对齐 | ✅ 编码 | ✅ 编码 |
| 字段名称 | ❌ 不编码 | ✅ 编码 |
| 继承层次 | ❌ 展平 | ✅ 保留 |
| 指针宽度/字节序 | ✅ 架构前缀 | ✅ 架构前缀 |

在 IPC 场景中，P_B 只需确保它读到的每个字节的含义与 P_A 写入时一致。
这恰好是 Layout 签名编码的全部信息——偏移、大小、对齐、多态标记。
字段名称（Definition 层的额外信息）对于 raw memory access 毫无影响。

### 1.3 形式化保证链

```
layout_signatures_match<T_A, T_B>() == true
    ⟹ ⟦T_A⟧_L = ⟦T_B⟧_L                  [API 语义]
    ⟹ L_P(T_A) = L_P(T_B)                  [Theorem 3.1 — Encoding Faithfulness]
    ⟹ T_A ≅_mem T_B                         [Definition 1.9]
    ⟹ sizeof(T_A) = sizeof(T_B)
       ∧ alignof(T_A) = alignof(T_B)
       ∧ fields_P(T_A) = fields_P(T_B)      [L_P 展开]
    ⟹ memcpy(shm, &obj_A, sizeof(T_A))
       可安全以 T_B 读取                      [字节级等价]
```

**关键定理：** Theorem 4.1 (Soundness) — 签名匹配 ⟹ memcmp 兼容，零误报。

### 1.4 Definition 层的辅助价值

虽然 IPC 场景以 Layout 为主，但 Definition 层在以下情况下提供额外安全网：

**场景：独立编译的两侧发生结构漂移**

```cpp
// Version 1 (both processes)
struct SharedData { uint64_t timestamp; double value; };

// Version 2 (process A updated, process B still v1)
struct SharedData { uint64_t created_at; double measurement; };  // 重命名字段
```

- `layout_signatures_match` → **true**（字节布局相同）
- `definition_signatures_match` → **false**（字段名不同）

Definition 不匹配暗示**语义漂移** — 虽然字节兼容，但两侧对字段含义的理解可能已不同。
在 CI 管线中使用 Definition 检查作为"额外护栏"可以提前发现这类潜在问题。

**形式化基础：** Theorem 5.5 (Strict Refinement) — ker(⟦·⟧_D) ⊊ ker(⟦·⟧_L)，
即存在 Layout 匹配但 Definition 不匹配的类型对。

### 1.5 边界条件

| 边界 | 行为 | 保证 |
|------|------|------|
| **跨平台指针大小不同** | 架构前缀不同 (`[64-le]` vs `[32-le]`)，签名不匹配 | ✅ 自动检测 |
| **跨平台字节序不同** | 架构前缀不同 (`[64-le]` vs `[64-be]`)，签名不匹配 | ✅ 自动检测 |
| **padding 差异** | 偏移不同，签名不匹配 | ✅ 自动检测 |
| **含指针的结构体** | `ptr[s:8,a:8]` 在两侧相同，但指针值无意义 | ⚠️ 签名匹配但语义不安全 |
| **含虚函数的类型** | `vptr` 标记匹配，但 vtable 指针跨进程无效 | ⚠️ 签名匹配但运行时不安全 |

**重要限制：** Layout 签名保证字节布局一致，但**不**保证数据在语义上可用。
含指针或虚函数表的类型即使签名匹配，也不能安全跨进程共享（指针指向的地址空间不同）。
这不是 TypeLayout 的缺陷，而是 IPC 的固有限制 — TypeLayout 的职责是验证布局，
不是验证数据内容。

### 1.6 与传统方法对比

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

### 2.2 推荐层：Layout（V1）

**核心理由：wire format 是纯字节序列，不关心字段名或继承。**

网络协议验证与 IPC 类似，关心的是"发送的字节序列能否被对方正确解析"。
Layout 签名精确编码了每个字段的偏移和类型，这正是 wire format 一致性的定义。

### 2.3 字节序检测与边界

TypeLayout 在架构前缀中编码字节序：

```
[64-le]record[s:16,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2],...}  // x86_64
[64-be]record[s:16,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2],...}  // SPARC64
```

**字节序不同 ⟹ 签名不同 ⟹ 布局"不兼容"。**

这是正确的行为：虽然结构体的偏移和大小相同，但 big-endian 机器上 `uint32_t` 的
字节排列与 little-endian 不同。TypeLayout 正确地将这视为"不兼容"。

**关键边界：检测 ≠ 转换。**

TypeLayout 检测到字节序差异（签名不匹配），但**不执行字节序转换**。
这是有意的职责分离：

```
TypeLayout: "这两个类型在不同字节序平台上，布局不兼容。"
用户:       "好的，我需要使用 ntohl/htonl 进行转换。"
```

对于网络协议，推荐做法是：
1. 使用 TypeLayout 确保**同平台**内的 wire format 一致性
2. 使用 `htonl/ntohl` 等函数处理跨字节序转换
3. 或者使用定宽类型 + 约定字节序（如网络字节序 = big-endian）

### 2.4 完备性优势

**传统方法：**

```cpp
// 手动验证 — 必须逐字段列出，且容易遗漏
static_assert(sizeof(PacketHeader) == 16);
static_assert(offsetof(PacketHeader, magic) == 0);
static_assert(offsetof(PacketHeader, version) == 4);
static_assert(offsetof(PacketHeader, type) == 6);
static_assert(offsetof(PacketHeader, payload_len) == 8);
static_assert(offsetof(PacketHeader, checksum) == 12);
// 新增字段时必须手动更新！
```

**TypeLayout 方法：**

```cpp
// 自动完备验证 — 一行代码覆盖所有字段
static_assert(layout_signatures_match<ClientPacketHeader, ServerPacketHeader>(),
    "Wire format mismatch between client and server!");
```

**形式化完备性论证：**

Layout 签名由递归遍历所有 `nonstatic_data_members_of` 生成（Definition 2.1）。
P2996 反射 API 保证枚举了所有成员（公理性信任编译器）。因此：

- 添加新字段 → 签名自动变化 → 不匹配被检测到 ✅
- 修改字段类型 → 签名自动变化 → 不匹配被检测到 ✅
- 修改字段顺序 → 偏移可能变化 → 签名自动变化 ✅
- 修改 pragma pack → 偏移变化 → 签名自动变化 ✅

手动 `static_assert` 无法提供这种自动完备性。

### 2.5 形式化保证

```
layout_signatures_match<ClientHdr, ServerHdr>() == true
    ⟹ ⟦ClientHdr⟧_L = ⟦ServerHdr⟧_L
    ⟹ L_P(ClientHdr) = L_P(ServerHdr)       [Theorem 3.1]
    ⟹ fields_P(ClientHdr) = fields_P(ServerHdr)
    ⟹ 每个字段在相同偏移处，相同大小，相同类型
    ⟹ send() 的字节序列可被 recv() 正确解析  [Theorem 4.1]
```

---

## 3. 文件格式兼容

### 3.1 场景描述

一个应用程序使用固定大小的文件头写入二进制文件（如传感器日志、数据库文件、
自定义配置文件）。文件头需要在以下维度保持兼容：

1. **跨平台兼容**：Linux 上写入的文件能在 macOS 上读取
2. **跨版本兼容**：v1 软件写入的文件能被 v2 软件读取
3. **语义一致性**：字段含义没有发生静默变化

**这是两层签名互补价值的最佳展示场景。**

### 3.2 推荐层：Layout（V1）+ Definition（V2）双层互补

| 验证目标 | 使用层 | 理由 |
|---------|--------|------|
| 跨平台字节兼容（能否 fread） | Layout (V1) | 字节布局一致 ⟹ fread 安全 |
| 版本演化检测（结构是否变了） | Definition (V2) | 字段名/继承变化 ⟹ 语义可能变了 |
| 综合判断 | 两层组合 | Layout 匹配 + Definition 匹配 = 最强保证 |

### 3.3 Layer 1：Layout 保证跨平台 fread 安全

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
    ⟹ L_P(LinuxHdr) = L_P(MacOSHdr)        [Theorem 3.1]
    ⟹ LinuxHdr ≅_mem MacOSHdr               [Definition 1.9]
    ⟹ fwrite(&hdr, sizeof(FileHeader), 1, f) 在 Linux 上写入的字节
       可以被 fread(&hdr, sizeof(FileHeader), 1, f) 在 macOS 上正确读取
```

注意：这要求两个平台的架构前缀相同（相同指针宽度和字节序），
否则签名不匹配会**正确地**拒绝兼容性判断。

### 3.4 Layer 2：Definition 检测版本演化

**典型案例：字段重命名**

```cpp
// Version 1
struct FileHeader_v1 {
    char     magic[4];
    uint32_t version;
    uint64_t timestamp;
    uint32_t entry_count;   // ← 原始名称
    uint32_t reserved;
};

// Version 2 (重命名了一个字段)
struct FileHeader_v2 {
    char     magic[4];
    uint32_t version;
    uint64_t timestamp;
    uint32_t num_records;   // ← 重命名: entry_count → num_records
    uint32_t reserved;
};
```

分析结果：

| 检查 | 结果 | 含义 |
|------|------|------|
| `layout_signatures_match<v1, v2>()` | **true** | 字节布局完全相同，可以 fread |
| `definition_signatures_match<v1, v2>()` | **false** | `@16[entry_count]` ≠ `@16[num_records]` |

**解读：**
- V1 回答："能安全读取这个文件吗？" → **是**（字节兼容）
- V2 回答："结构定义变了吗？" → **是**（字段名变了）

这给开发者的指导是：v2 可以安全读取 v1 的文件数据，但代码中对字段的引用
需要更新（`hdr.entry_count` → `hdr.num_records`）。

**形式化基础：**

```
⟦v1⟧_L = ⟦v2⟧_L ∧ ⟦v1⟧_D ≠ ⟦v2⟧_D
    ⟹ (v1, v2) ∈ ker(⟦·⟧_L) \ ker(⟦·⟧_D)      [Theorem 5.5 的实例]
```

这正是 Theorem 5.5 (Strict Refinement) 描述的现象——Definition 等价核
严格小于 Layout 等价核。

### 3.5 典型决策矩阵

| 情况 | Layout | Definition | 行动 |
|------|:------:|:----------:|------|
| 两层都匹配 | ✅ | ✅ | 完全兼容，无需任何操作 |
| Layout 匹配，Definition 不匹配 | ✅ | ❌ | 字节兼容但结构变了；审查代码变更 |
| Layout 不匹配 | ❌ | — | 字节不兼容；需要格式迁移 |

注意：由 V3 投影定理（Theorem 5.4），Definition 匹配 ⟹ Layout 匹配。
因此"Definition 匹配但 Layout 不匹配"是不可能的。

---

## 4. 插件 ABI / ODR 检测

### 4.1 场景描述

宿主程序通过 `dlopen` (Linux) 或 `LoadLibrary` (Windows) 动态加载插件。
宿主和插件通过共享的结构体类型交换数据。两种部署模式：

1. **联合编译**：宿主和插件在同一个构建系统中编译
2. **独立编译**：插件由第三方开发者独立编译

### 4.2 推荐层：两层组合（V1 ABI + V2 ODR）

| 检测目标 | 使用层 | 能力 |
|---------|--------|------|
| ABI 兼容性（能否安全传递数据） | Layout (V1) | 检测字节布局不匹配 |
| ODR 违规（同名不同定义） | Definition (V2) | 检测结构定义差异 |

### 4.3 Layer 1：Layout 验证 ABI 兼容性

当宿主调用插件函数并传递结构体指针时：

```cpp
// Host
struct PluginConfig { uint32_t version; uint64_t flags; char name[32]; };
plugin->init(&config);

// Plugin (独立编译)
void init(const PluginConfig* cfg) {
    uint64_t f = cfg->flags;  // 安全吗？依赖字节布局一致
}
```

如果宿主和插件的 `PluginConfig` 字节布局不同（例如不同编译器选项导致
不同的 padding），`cfg->flags` 会读到错误的内存位置 → 未定义行为。

**Layout 签名保证：**

```
layout_signatures_match<Host::PluginConfig, Plugin::PluginConfig>() == true
    ⟹ 两侧 PluginConfig 字节布局相同
    ⟹ cfg->flags 在两侧指向相同偏移
    ⟹ 数据传递安全                          [Theorem 4.1]
```

**运行时验证模式（独立编译场景）：**

```cpp
// Plugin 导出签名字符串
extern "C" const char* get_config_layout_sig() {
    static constexpr auto sig = get_layout_signature<PluginConfig>();
    return sig.data();
}

// Host 在 dlopen 后验证
auto get_sig = dlsym(handle, "get_config_layout_sig");
if (strcmp(get_sig(), host_config_sig) != 0) {
    // ABI 不兼容，拒绝加载
    dlclose(handle);
    return Error::ABIMismatch;
}
```

### 4.4 Layer 2：Definition 检测 ODR 违规

**ODR (One Definition Rule) 问题：**

C++ 标准要求同一程序中同名类型的定义必须完全相同。然而，当宿主和插件
独立编译时，编译器和链接器**无法**跨翻译单元检测 ODR 违规。

```cpp
// Host's config.h (v1)
struct Config {
    uint32_t timeout_ms;
    uint32_t max_retries;
};

// Plugin's config.h (v2 — 独立开发者修改了)
struct Config {
    uint32_t timeout_seconds;   // 重命名！语义变了
    uint32_t retry_limit;       // 重命名！语义变了
};
```

这两个 `Config` 的字节布局**完全相同**（两个 uint32_t），所以：

| 检查 | 结果 | 含义 |
|------|------|------|
| `layout_signatures_match` | **true** | ABI 兼容，数据传递不会崩溃 |
| `definition_signatures_match` | **false** | 结构定义不同 — ODR 违规！ |

**ODR 违规的严重性：** 虽然 ABI 兼容意味着不会立即崩溃，但宿主以 `timeout_ms`
的语义写入数据，插件以 `timeout_seconds` 的语义读取 — 可能导致超时时间差 1000 倍。

**Definition 层的独特价值：** 这种 ODR 违规是**编译器和链接器无法检测的**
（因为它们在不同的编译单元中）。TypeLayout 的 Definition 签名是目前唯一
能在编译时或加载时检测此类问题的工具。

### 4.5 两种验证模式

```
                    ┌─────────────────────────┐
                    │   Plugin Architecture   │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
            ┌───── │  Co-compiled or Indep?   │ ─────┐
            │      └─────────────────────────┘       │
      Co-compiled                              Independent
            │                                        │
    ┌───────▼────────┐                    ┌──────────▼─────────┐
    │ static_assert   │                    │ Runtime strcmp      │
    │ definition_     │                    │ via dlsym           │
    │ signatures_     │                    ├──────────┬─────────┤
    │ match<H,P>()   │                    │ Layout   │ Def(推荐)│
    └───────┬────────┘                    └─────┬────┴────┬────┘
            │                                   │         │
    V1 + V2 保证                          V1 保证    V1+V2 保证
    (编译时最强)                        (ABI only)  (ABI+ODR)
```

**V3 投影定理的实际意义：**

Theorem 5.4 保证 Definition 匹配 ⟹ Layout 匹配。因此：
- **联合编译**：`static_assert(definition_signatures_match<H,P>())` — 编译时 V1+V2 保证
- **独立编译**：运行时通过 `dlsym` 导出签名字符串进行 `strcmp` 比对。
  导出哪一层取决于安全需求：
  - 导出 Layout 签名 → 仅 V1 保证（ABI 兼容性）
  - 导出 Definition 签名 → V1+V2 保证（ABI + ODR 检测，因为 V3 投影）
  - **推荐导出 Definition 签名**——一个字符串同时包含两层保证

### 4.6 形式化正确性分析

**命题：Definition 不匹配 ⟹ ODR 违规（在同名类型的前提下）**

严格来说，TypeLayout 执行**结构分析**（不包含类型自身名称），所以
Definition 不匹配的准确含义是：

    ⟦T_H⟧_D ≠ ⟦T_P⟧_D ⟹ D_P(T_H) ≠ D_P(T_P)    [Corollary 3.2.1 的逆否]

即两个类型的结构树不同（字段名、基类、限定名等至少有一项不同）。

如果这两个类型**恰好同名**（宿主和插件都叫 `Config`），那么 D_P 不同
意味着同名类型有不同的定义 — 这就是 ODR 违规。

**注意：** TypeLayout 无法检测两个**不同名**但应该相同的类型之间的差异
（因为签名不包含类型自身名称）。这是结构分析的固有限制。

---

## 5. 序列化版本检查

### 5.1 场景描述

序列化框架（JSON、Protocol Buffers、自定义格式）在反序列化时需要确保
数据结构与写入时一致。序列化通常基于**字段名**映射：

```cpp
// JSON 序列化 — 基于字段名
{"timeout_ms": 5000, "max_retries": 3}

// 如果结构体字段名变了，反序列化会失败或产生错误映射
```

### 5.2 推荐层：Definition（V2 为主）

**核心理由：序列化框架依赖字段名和类型，不是字节偏移。**

| 序列化方式 | 依赖的信息 | 适合的层 |
|-----------|-----------|---------|
| JSON (基于名称) | 字段名、类型 | Definition (V2) |
| Protocol Buffers (基于字段号) | 字段号、类型 | Definition (V2)* |
| 自定义二进制 (fwrite) | 字节偏移、大小 | Layout (V1) |
| MessagePack (基于顺序) | 字段顺序、类型 | Definition (V2) |

*注：Protobuf 使用 field_number 而非字段名，但 TypeLayout Definition
  层的字段名和顺序变化仍能指示结构变更。

### 5.3 Layer 2：Definition 检测语义变更

```cpp
// Version 1
struct Config {
    uint32_t timeout_ms;
    std::string name;
};

// Version 2 (语义变更)
struct Config {
    uint32_t timeout_seconds;  // 从毫秒改为秒！
    std::string label;         // 从 name 改为 label！
};
```

Definition 签名对比：

```
v1: record[s:40,a:8]{@0[timeout_ms]:u32[s:4,a:4],@8[name]:...}
v2: record[s:40,a:8]{@0[timeout_seconds]:u32[s:4,a:4],@8[label]:...}
```

`definition_signatures_match` → **false**

- `[timeout_ms]` ≠ `[timeout_seconds]` — 检测到字段重命名
- `[name]` ≠ `[label]` — 检测到字段重命名

**意义：** 序列化逻辑使用字段名作为 key。字段名变更意味着旧序列化数据
无法被新结构体正确反序列化。Definition 签名不匹配正确地发出警告。

### 5.4 Layer 1：Layout 检测二进制序列化不兼容

对于 `fwrite` 风格的原始二进制序列化，Layout 层更为直接：

```cpp
// 如果结构体中间插入了新字段
struct ConfigV3 {
    uint32_t timeout_ms;
    uint32_t priority;     // 新增字段！
    std::string name;
};
```

- `layout_signatures_match<ConfigV1, ConfigV3>()` → **false**（偏移变了）
- Layout 不匹配意味着 `fread` 一个 v1 文件的数据到 v3 结构体会出错

### 5.5 两层互补的判断矩阵

| Layout | Definition | 序列化安全性判断 |
|:------:|:----------:|-----------------|
| ✅ | ✅ | 完全兼容 — 所有序列化方式安全 |
| ✅ | ❌ | 二进制兼容但语义变了 — 名称映射序列化可能出错 |
| ❌ | — | 二进制不兼容 — 原始二进制序列化必然出错 |

### 5.6 与其他版本管理方案的对比

| 方案 | 检测能力 | 自动性 | 运行时开销 |
|------|---------|--------|-----------|
| Protobuf field_number | 字段号冲突 | 手动管理 | 序列化/反序列化开销 |
| JSON Schema versioning | schema 差异 | 需要维护 schema | 解析开销 |
| 手动版本号 | 版本号变化 | 完全手动 | 无 |
| **TypeLayout Definition** | 字段名/类型/继承变化 | **全自动** | **零**（编译时） |

TypeLayout 的独特优势：**零运行时开销的全自动检测**。检测发生在编译时，
不需要维护额外的 schema 或版本号。

### 5.7 形式化保证

```
definition_signatures_match<ConfigV1, ConfigV2>() == true
    ⟹ ⟦V1⟧_D = ⟦V2⟧_D
    ⟹ D_P(V1) = D_P(V2)                    [Theorem 3.2]
    ⟹ 字段名、类型、偏移、继承层次完全相同
    ⟹ 所有序列化方式安全                     [Corollary 3.2.1]
```

---

## 6. API 兼容检查

### 6.1 场景描述

一个库的公共 API 使用结构体作为参数和返回类型。库升级时，这些结构体可能
发生变化。需要区分两个层次的兼容性：

- **ABI 兼容**：旧代码链接新库后能正确运行（字节级）
- **API 兼容**：旧代码使用新库的头文件后能正确编译和运行（语义级）

### 6.2 推荐层：Definition（V2 为主）

**核心理由：API 兼容关心语义级结构一致性，不仅是字节。**

| 兼容性类型 | 关心的信息 | TypeLayout 映射 |
|-----------|-----------|----------------|
| ABI 兼容 | 字节布局、偏移、大小 | Layout (V1) |
| API 兼容 | 字段名、类型、继承层次 | Definition (V2) |

### 6.3 ABI 兼容 vs API 兼容的精确区分

**案例：继承层次重构**

```cpp
// Library v1
struct Result {
    int error_code;
    char message[256];
};

// Library v2 (重构为继承)
struct ErrorBase { int error_code; };
struct Result : ErrorBase {
    char message[256];
};
```

如果编译器将 `ErrorBase` 的 `error_code` 放在与 v1 相同的偏移处（通常如此），
则：

| 检查 | 结果 | 含义 |
|------|------|------|
| `layout_signatures_match<v1::Result, v2::Result>()` | **true** | ABI 兼容：旧二进制可以链接新库 |
| `definition_signatures_match<v1::Result, v2::Result>()` | **false** | API 变了：v2 有 `~base<ErrorBase>` |

**解读：**

- **ABI 层面：** 安全。旧编译的客户端代码调用新库不会崩溃，因为字节布局一致。
- **API 层面：** 需要审查。客户端代码中 `result.error_code` 仍然能编译
  （继承使 `error_code` 可访问），但 `dynamic_cast<ErrorBase*>(&result)` 的行为
  在 v1 和 v2 之间不同。

### 6.4 继承层次变更检测

Definition 签名的独特能力之一是检测继承层次变更：

```
v1::Result: record[s:260,a:4]{@0[error_code]:i32[s:4,a:4],@4[message]:bytes[s:256,a:1]}
v2::Result: record[s:260,a:4]{~base<ErrorBase>:record[s:4,a:4]{@0[error_code]:i32[s:4,a:4]},@4[message]:bytes[s:256,a:1]}
```

`~base<ErrorBase>:` 前缀表明 v2 有继承关系。Layout 层将继承展平为偏移序列，
看不到这个差异；但 Definition 层保留了完整的继承树。

**形式化基础：**

这又是 Theorem 5.5 (Strict Refinement) 的实例：
- v1 和 v2 的 Layout 签名相同（展平后字节布局一致）
- v1 和 v2 的 Definition 签名不同（继承结构不同）
- (v1, v2) ∈ ker(⟦·⟧_L) \ ker(⟦·⟧_D)

### 6.5 结构分析的设计选择影响

TypeLayout 执行**结构分析**（Structural Analysis），不包含类型自身名称：

```cpp
struct Point { int32_t x, y; };
struct Coord { int32_t x, y; };  // 不同类型名，相同结构

definition_signatures_match<Point, Coord>() == true  // ！
```

**影响分析：**

| 分析类型 | 结论 | 适用场景 |
|---------|------|---------|
| 结构分析（TypeLayout） | "结构等价" | 关心"能否互换使用" |
| 名义分析（类型系统） | "不同类型" | 关心"是否是同一类型" |

对于 API 兼容检查，这意味着：

- TypeLayout 能回答："这两个结构体**能否互换**？" ✅
- TypeLayout **不能**回答："这是否是**同一个**类型？" ❌

如果需要名义身份检查（type identity），用户应组合 TypeLayout 与 `typeid`
或类型名称比较。这是有意的设计选择——TypeLayout 关注结构等价性，
因为这才是跨编译单元的真实安全保证。

### 6.6 API 兼容性验证策略

```
                API 兼容性检查策略
                
        ┌───────────────────────────┐
        │ definition_signatures_    │
        │ match<OldAPI, NewAPI>()   │
        └─────────┬────┬───────────┘
                  │    │
            match │    │ mismatch
                  │    │
          ┌───────▼┐  ┌▼──────────────┐
          │完全兼容│  │检查 Layout    │
          │API+ABI │  │签名是否匹配   │
          └────────┘  └───┬───┬───────┘
                         │   │
                   match │   │ mismatch
                         │   │
                 ┌───────▼┐ ┌▼──────────┐
                 │ABI兼容  │ │完全不兼容  │
                 │API变了  │ │需要迁移    │
                 │审查代码 │ └───────────┘
                 └────────┘
```

### 6.7 形式化保证

```
definition_signatures_match<OldAPI, NewAPI>() == true
    ⟹ D_P(Old) = D_P(New)                   [Theorem 3.2 / Corollary 3.2.1]
    ⟹ 字段名、类型、继承层次完全一致
    ⟹ API 结构兼容

    ⟹ (by Theorem 5.4 / V3 Projection)
       layout_signatures_match<OldAPI, NewAPI>() == true
    ⟹ ABI 也兼容
```

一个 Definition 匹配同时包含 V1 + V2 两层保证。

---

## 7. 场景总览矩阵

### 7.1 层选择总览

| 场景 | 主要层 | 辅助层 | 核心定理 | 选择理由 |
|------|:------:|:------:|---------|---------|
| IPC / 共享内存 | Layout (V1) | Definition (V2) | Thm 4.1 | 关心字节布局，不关心名称 |
| 网络协议验证 | Layout (V1) | — | Thm 4.1 | wire format 是纯字节序列 |
| 文件格式兼容 | Layout (V1) | Definition (V2) | Thm 4.1 + 4.3 | 字节兼容 + 版本演化检测 |
| 插件 ABI/ODR | Layout (V1) | Definition (V2) | Thm 4.1 + 3.5.1 | ABI 验证 + ODR 检测 |
| 序列化版本检查 | Definition (V2) | Layout (V1) | Thm 3.2 | 序列化依赖字段名和类型 |
| API 兼容检查 | Definition (V2) | Layout (V1) | Thm 3.2 + 4.2 | API 关心语义级一致性 |

### 7.2 核心价值映射

| Core Value | 覆盖场景 | 意义 |
|------------|---------|------|
| **V1** (Layout 可靠性) | IPC、网络、文件格式、插件 ABI | 字节级安全保证 |
| **V2** (Definition 精确性) | 序列化、API 检查、ODR 检测 | 语义级安全保证 |
| **V3** (投影关系) | 插件（联合编译）、API 检查 | 一个 Definition 匹配 = 两层保证 |

### 7.3 形式化保证总览

| 场景 | 安全判据 | 形式化表达 |
|------|---------|-----------|
| IPC | memcpy 安全 | `⟦T_A⟧_L = ⟦T_B⟧_L ⟹ T_A ≅_mem T_B` (Thm 4.1) |
| 网络 | wire format 一致 | `⟦Client⟧_L = ⟦Server⟧_L ⟹ 相同偏移/大小` (Thm 3.1) |
| 文件 | fread 安全 + 版本一致 | V1: Thm 4.1, V2: Cor 3.2.1 |
| 插件 | ABI 安全 + ODR 一致 | V1: Thm 4.1, V2: Cor 3.2.1, 对比: Thm 5.5 |
| 序列化 | 结构一致 | `⟦V1⟧_D = ⟦V2⟧_D ⟹ D_P(V1) = D_P(V2)` (Cor 3.2.1) |
| API | 语义兼容 | `⟦Old⟧_D = ⟦New⟧_D ⟹ API 结构兼容` (Thm 3.2 + 4.2) |

### 7.4 两层互补的设计合理性

**为什么不只用一层？**

- **只用 Layout：** 无法检测语义漂移（字段重命名、继承重构）。在序列化和
  API 场景中，这些变化可能导致静默错误。
- **只用 Definition：** 过于严格。IPC 和网络协议不关心字段名，只关心字节布局。
  使用 Definition 会导致不必要的误报（两个结构体字段名不同但字节布局相同）。

**两层设计的 sweet spot：**

```
             strictness →
    ┌────────────────────────────────────┐
    │                                    │
    │  Layout  ─── "字节安全" ───────→   │  (宽松：只看字节)
    │                                    │
    │  Definition  ── "结构安全" ────→   │  (严格：看名称+层次)
    │                                    │
    └────────────────────────────────────┘
    
    IPC/网络: Layout 足矣
    序列化/API: Definition 必须
    文件/插件: 两层互补最佳
```

这种设计让用户根据场景选择合适的严格程度，避免了"一刀切"的局限。
V3 投影定理保证两层之间的一致性——严格层（Definition）总是蕴含宽松层（Layout）。

---

## 8. 正确性与完备性验证

本节对每个场景中两层签名的**正确性**（Correctness: 签名保证是否确实成立）和
**完备性**（Completeness: 签名是否捕获了该场景所需的全部信息）进行系统性审计。

### 验证方法论

对每个场景 S，定义：

- **安全条件 Safety(S)**：该场景下数据交换安全的充要条件
- **Layout 捕获集 Cap_L**：Layout 签名编码的信息集合
- **Definition 捕获集 Cap_D**：Definition 签名编码的信息集合（Cap_D ⊃ Cap_L）

**正确性** = Cap_X ⊇ Safety(S) 所需信息 ⟹ 签名匹配确实保证安全
**完备性** = Cap_X 是否遗漏了 Safety(S) 要求的某些信息

### 8.1 IPC / 共享内存

**Safety(IPC)** = 两侧的类型在字节级完全等价，即：
- 相同 sizeof、alignof
- 每个字段在相同字节偏移处
- 每个字段具有相同的原始类型和大小
- 相同的多态性状态（vptr 存在与否影响 sizeof 和字段偏移）

**Layout 层正确性验证：**

| Safety 要素 | Layout 是否编码 | 实现位置 | 判定 |
|------------|:--------------:|---------|:----:|
| sizeof(T) | ✅ `s:SIZE` | `to_fixed_string(sizeof(T))` (line 516) | ✅ |
| alignof(T) | ✅ `a:ALIGN` | `to_fixed_string(alignof(T))` (line 518) | ✅ |
| 字段偏移 | ✅ `@OFF:` | `offset_of(member).bytes + OffsetAdj` (line 197/206/210) | ✅ |
| 字段类型+大小 | ✅ 递归 `TypeSignature` | `TypeSignature<FieldType, Layout>::calculate()` (line 203/212) | ✅ |
| 多态标记 | ✅ 合成 `ptr[s:N,a:N]` | `introduces_vptr<T>` → synthesized `@0:ptr[s:N,a:N]` field | ✅ |
| 基类展平 | ✅ 递归展平 | `layout_all_prefixed<BaseType, offset>()` (line 227) | ✅ |
| 嵌套 struct 展平 | ✅ 递归展平 | `layout_all_prefixed<FieldType, field_offset>()` (line 207) | ✅ |
| union 不展平 | ✅ 原子保留 | `get_layout_union_content<T>()` (line 507) | ✅ |
| 位域 | ✅ `@BYTE.BIT:bits<W,T>` | `is_bit_field` + `bit_size_of` (line 194-204) | ✅ |
| 字节序 | ✅ 架构前缀 `[64-le]` | `get_arch_prefix()` (signature.hpp line 13-18) | ✅ |
| 指针宽度 | ✅ 架构前缀 `[64-...]` | `sizeof(void*) == 8` check (signature.hpp line 13) | ✅ |

**Layout 正确性结论：✅ 正确。** Layout 签名编码了 Safety(IPC) 的全部要素。

**Layout 完备性验证：**

| 可能遗漏 | 分析 | 影响 |
|---------|------|------|
| padding 字节 | padding 隐含于相邻字段偏移的间隙中；`sizeof` 包含尾部 padding | ✅ 隐式完备 |
| 空基类优化 (EBO) | EBO 影响 `offset_of` 和 `sizeof`，两者都被编码 | ✅ 已覆盖 |
| `[[no_unique_address]]` | 编码的是编译器报告的 `offset_of`，可能重叠 | ⚠️ 忠实编码，但语义需用户理解 |
| 虚基类偏移 | Layout 展平虚基类，偏移由编译器提供 | ✅ 已覆盖 |
| 指针/引用成员的值 | 签名仅编码类型和大小，不编码值 | ⚠️ 非签名职责（已在 §1.5 说明） |

**Layout 完备性结论：✅ 完备（在签名职责范围内）。** 唯一的"不完备"项（指针值、vptr 值）
不属于布局信息，而是数据内容——这超出了类型布局签名的职责边界。

**Definition 层在 IPC 中的正确性：**

Definition 层编码了 Layout 的全部信息**加上**字段名和继承结构。
由 V3 投影定理（Theorem 5.4），Definition 匹配 ⟹ Layout 匹配。
因此 Definition 匹配对于 IPC 是**充分条件**（但非必要——字段名不同不影响 IPC 安全）。

**结论：Definition 用于 IPC 正确但过于严格。** Layout 是更精确的匹配。

---

### 8.2 网络协议验证

**Safety(Network)** = 发送端和接收端的报文头在**相同平台上**字节布局完全一致。
跨平台时还需要字节序一致（或用户自行转换）。

**Layout 层正确性验证：**

与 IPC 相同——Layout 编码了全部字段偏移、大小、类型。

额外验证项：

| Safety 要素 | Layout 是否编码 | 判定 |
|------------|:--------------:|:----:|
| 字节序 | ✅ `[64-le]`/`[64-be]` | ✅ |
| 同平台 wire format 一致 | ✅ 相同平台相同签名 | ✅ |
| 跨平台字节序转换 | ❌ 不执行转换 | ⚠️ 非签名职责 |

**Layout 完备性验证（网络特有）：**

| 可能遗漏 | 分析 | 影响 |
|---------|------|------|
| 网络字节序约定 | TypeLayout 不强制网络字节序；签名反映本机字节序 | ⚠️ 用户责任，已在 §2.3 说明 |
| `#pragma pack` 效果 | `#pragma pack` 改变偏移，签名通过 `offset_of` 自动反映 | ✅ 已覆盖 |
| 结构体末尾 padding | `sizeof` 包含末尾 padding，签名编码之 | ✅ 已覆盖 |
| 位域跨编译器差异 | 位域布局是实现定义的；同编译器内一致 | ⚠️ 已知限制（§5.7 caveat） |

**完备性结论：✅ 完备（同平台）。** 跨平台字节序转换不是签名的职责。

---

### 8.3 文件格式兼容

**Safety(FileFormat)** 分两层：
- **字节安全** = 跨平台 fread/fwrite 一致（同 IPC Safety）
- **语义安全** = 结构定义（字段名、类型、继承）跨版本一致

**Layout 层正确性（字节安全）：**

与 IPC 完全相同。✅ 正确且完备。

**Definition 层正确性（语义安全）：**

| Safety 要素 | Definition 是否编码 | 实现位置 | 判定 |
|------------|:------------------:|---------|:----:|
| 字段名 | ✅ `[name]` | `get_member_name<member, Index>()` (line 50-61) | ✅ |
| 字段偏移 | ✅ `@OFF` | `offset_of(member).bytes` (line 99) | ✅ |
| 字段类型（递归） | ✅ 递归 `TypeSignature<..., Definition>` | line 95/102 | ✅ |
| 继承结构 | ✅ `~base<QNAME>:/~vbase<QNAME>:` | `definition_base_signature` (line 131-142) | ✅ |
| 基类限定名 | ✅ `qualified_name_for<>()` | line 26-37 | ✅ |
| 多态标记 | ✅ `,polymorphic` | `is_polymorphic_v<T>` → `,polymorphic]` (line 535) | ✅ |
| 枚举限定名 | ✅ `enum<QNAME>` | `get_type_qualified_name<T>()` (line 484-485) | ✅ |
| 匿名成员 | ✅ `<anon:N>` | `has_identifier` check → fallback (line 52-59) | ✅ |

**Definition 完备性验证（版本演化特有）：**

| 可能遗漏 | 分析 | 影响 |
|---------|------|------|
| 字段默认值变更 | 签名不编码默认值（编译时常量） | ⚠️ 超出签名职责 |
| 注释/文档变更 | 签名不编码注释 | ✅ 正确不编码 |
| 方法签名变更 | 只反射 `nonstatic_data_members`，不含方法 | ⚠️ 设计选择（成员函数不影响布局） |
| access specifier 变更 | `access_context::unchecked()` 忽略访问控制 | ⚠️ 不检测 private→public 变更 |
| 类型自身名称变更 | 签名不含类型自身名称（结构分析） | ⚠️ 已在 §6.5 说明 |

**完备性结论：✅ 对数据布局和结构相关的语义变更完备。** 不编码方法、默认值、访问控制
等非数据布局信息——这些超出了类型布局签名的设计范围，是有意的设计选择。

---

### 8.4 插件 ABI / ODR 检测

**Safety(Plugin)** 分两层：
- **ABI 安全** = 宿主和插件的共享结构体字节布局一致
- **ODR 安全** = 同名类型具有相同的完整定义

**Layout 层正确性（ABI 安全）：**

与 IPC 相同。✅ 正确且完备。

**Definition 层正确性（ODR 安全）：**

C++ ODR 要求同名类型的定义"token-by-token identical"。Definition 签名不检查
token 级别的等价，而是检查**结构级别**的等价：

| ODR 等价要素 | Definition 是否检测 | 判定 |
|-------------|:------------------:|:----:|
| 字段名相同 | ✅ `[name]` | ✅ |
| 字段类型相同 | ✅ 递归类型签名 | ✅ |
| 字段顺序相同 | ✅ `offset_of` 顺序 | ✅ |
| 基类列表相同 | ✅ `~base<QNAME>:` | ✅ |
| 虚/非虚继承相同 | ✅ `~base` vs `~vbase` | ✅ |
| 枚举底层类型相同 | ✅ `enum<QNAME>[...]<underlying>` | ✅ |
| sizeof/alignof 相同 | ✅ `s:SIZE,a:ALIGN` | ✅ |
| 成员函数签名相同 | ❌ 不检测 | ⚠️ |
| 静态成员相同 | ❌ 不检测（仅非静态数据成员） | ⚠️ |
| 嵌套类型定义相同 | ❌ 不直接检测 | ⚠️ |
| 模板参数相同 | ❌ 不检测（结构分析） | ⚠️ |
| 类型自身名称相同 | ❌ 不编码（结构分析） | ⚠️ |
| access specifier | ❌ `unchecked` | ⚠️ |

**ODR 检测完备性结论：⚠️ 部分完备。**

Definition 签名检测**数据布局相关的** ODR 违规（字段名/类型/偏移/继承变更），
但**不检测**成员函数、静态成员、嵌套类型、模板参数等非数据布局变更。

**正确性论证：** Definition 签名的 ODR 检测是**保守正确**的：

- **无误报 (Sound)**：Definition 签名匹配 ⟹ D_P 完全相同（Corollary 3.2.1）
  ⟹ 数据布局方面的 ODR 一致。签名匹配不会错误地放过数据布局不同的类型。

- **有漏报 (Incomplete)**：两个类型可能数据布局相同但成员函数不同（仍是 ODR 违规），
  此时 Definition 签名匹配但存在 ODR 问题。这是设计限制——TypeLayout 只反射
  数据成员，不反射方法。

**风险评估：**

```
                    ODR 违规类型
                         │
           ┌─────────────┼──────────────┐
           │             │              │
    数据布局变更     成员函数变更     其他变更
    (字段名/类型/    (虚函数/         (static/
     继承/偏移)      重载)            nested)
           │             │              │
    Definition ✅    Definition ❌    Definition ❌
    Layout ✅/❌     Layout ❌       Layout ❌
```

对于**插件场景**，数据布局相关的 ODR 违规是最危险的（直接导致数据损坏），
而 Definition 签名恰好覆盖了这类违规。成员函数相关的 ODR 违规（如虚函数表不一致）
需要其他工具检测（如 ABI checker）。

---

### 8.5 序列化版本检查

**Safety(Serialization)** = 序列化框架能正确映射新旧版本的字段。具体取决于序列化方式：

| 序列化方式 | Safety 条件 |
|-----------|------------|
| 基于名称 (JSON) | 字段名、字段类型一致 |
| 基于顺序 (MessagePack) | 字段顺序、字段类型一致 |
| 基于偏移 (fwrite) | 字段偏移、字段大小一致 |

**Definition 层正确性验证：**

| Safety 要素 | 基于名称 | 基于顺序 | Definition 是否编码 | 判定 |
|------------|:-------:|:-------:|:------------------:|:----:|
| 字段名 | ✅ 需要 | ❌ | ✅ `[name]` | ✅ |
| 字段类型 | ✅ 需要 | ✅ 需要 | ✅ 递归类型签名 | ✅ |
| 字段顺序 | ❌ | ✅ 需要 | ✅ 按声明顺序* | ✅ |
| 字段偏移 | ❌ | ❌ | ✅ `@OFF` | ✅ |
| 继承层次 | 视框架 | 视框架 | ✅ `~base<>` | ✅ |

*字段顺序：P2996 的 `nonstatic_data_members_of` 按声明顺序返回（§6.5 假设条件）。
Definition 签名按此顺序编码字段，因此字段重排会导致签名变化。

**Definition 完备性验证（序列化特有）：**

| 可能遗漏 | 分析 | 影响 |
|---------|------|------|
| 字段默认值 | 签名不编码；默认值变更可能影响反序列化行为 | ⚠️ 超出签名职责 |
| 可选字段标记 | 签名不区分必选/可选字段（如 `std::optional`） | ⚠️ `optional<T>` 会被编码为其内部布局 |
| 序列化特性注解 | 签名不编码 `[[deprecated]]` 等属性 | ⚠️ 超出签名职责 |
| protobuf field_number | 签名不编码 wire tag，仅编码 C++ 结构 | ⚠️ protobuf 不直接使用 C++ 字段名 |

**完备性结论：✅ 对名称映射序列化完备。** 对 protobuf 等 IDL-based 序列化框架，
TypeLayout 只能检测 C++ 侧的结构变更，无法检测 .proto 文件的 field_number 变更。
这是合理的——TypeLayout 分析的是 C++ 类型，不是外部 IDL。

**Layout 层辅助正确性：**

Layout 签名对于 `fwrite` 风格的二进制序列化是完备的（与 IPC 相同）。
对于名称映射序列化，Layout 匹配是必要条件但非充分条件：

```
Definition 匹配 ⟹ Layout 匹配 ⟹ 二进制序列化安全
Definition 匹配 ⟹ 名称/顺序序列化也安全
Layout 匹配 ⇏ 名称/顺序序列化安全（字段名可能不同）
```

---

### 8.6 API 兼容检查

**Safety(API)** = 库升级后，使用旧头文件编译的客户端代码仍能：
1. 正确编译（源码兼容）
2. 正确链接运行（二进制兼容）

**Definition 层正确性验证：**

| Safety 要素 | Definition 是否编码 | 判定 |
|------------|:------------------:|:----:|
| 字段名不变 | ✅ `[name]` | ✅ |
| 字段类型不变 | ✅ 递归签名 | ✅ |
| 继承关系不变 | ✅ `~base<>` / `~vbase<>` | ✅ |
| sizeof/alignof 不变 | ✅ `s:SIZE,a:ALIGN` | ✅ |
| 多态性不变 | ✅ `polymorphic` | ✅ |
| 枚举底层类型不变 | ✅ `enum<QNAME>[...]<U>` | ✅ |

**Definition 完备性验证（API 特有）：**

| 可能遗漏 | 分析 | 影响 |
|---------|------|------|
| 成员函数签名变更 | 不检测（仅反射数据成员） | ⚠️ API 破坏但签名不变 |
| 虚函数表变更 | 仅检测 `is_polymorphic` 状态，不检测 vtable 内容 | ⚠️ ABI 可能破坏 |
| 模板参数变更 | 结构分析，不区分 `vector<int>` 的模板参数 | ⚠️ 但布局差异会被检测 |
| 类型自身名称变更 | 不编码（结构分析设计） | ⚠️ 已在 §6.5 说明 |
| 新增/删除成员函数 | 非数据成员，不影响布局 | ⚠️ API 变更但非 ABI 变更 |
| access specifier 变更 | `unchecked` 忽略 | ⚠️ `private→public` 不检测 |

**完备性结论：⚠️ 对数据结构 API 完备，对成员函数 API 不完备。**

这是一个重要的边界：TypeLayout 验证的是**数据结构的 API 兼容性**，
不是**完整的 C++ API 兼容性**。完整的 API 兼容性检查还需要：

1. 虚函数表兼容性检查 → 需要 ABI checker 工具（如 `abi-compliance-checker`）
2. 模板实例化兼容性 → 需要专门的模板兼容性分析
3. 函数签名兼容性 → 需要符号级分析

TypeLayout 的价值在于：它是**唯一能在编译时自动检测数据结构 API 变更**的工具，
填补了其他工具难以覆盖的空白（P2996 反射提供了编译器级别的精确数据）。

---

### 8.7 正确性与完备性总览

| 场景 | 推荐层 | 正确性 | 完备性 | 未覆盖的安全要素 |
|------|:------:|:------:|:------:|-----------------|
| IPC / 共享内存 | Layout | ✅ 正确 | ✅ 完备 | 指针值/vptr 值（非签名职责） |
| 网络协议验证 | Layout | ✅ 正确 | ✅ 完备（同平台） | 字节序转换（非签名职责） |
| 文件格式（字节层） | Layout | ✅ 正确 | ✅ 完备 | 同 IPC |
| 文件格式（语义层） | Definition | ✅ 正确 | ✅ 完备 | 默认值、方法（非数据布局） |
| 插件 ABI | Layout | ✅ 正确 | ✅ 完备 | 同 IPC |
| 插件 ODR | Definition | ✅ 正确（保守） | ⚠️ 部分 | 成员函数、静态成员 ODR |
| 序列化（名称映射） | Definition | ✅ 正确 | ✅ 完备 | IDL field_number（非 C++ 结构） |
| 序列化（二进制） | Layout | ✅ 正确 | ✅ 完备 | 同 IPC |
| API（数据结构） | Definition | ✅ 正确 | ✅ 完备 | — |
| API（完整 C++） | Definition | ✅ 正确（保守） | ⚠️ 部分 | 虚函数表、方法签名 |

### 8.8 全局正确性定理

**定理 8.1 (场景安全性保守正确)。** 对于所有 6 个应用场景，如果推荐层的签名匹配，
则该场景的安全条件成立（在签名职责范围内）。形式化地：

    对于 S ∈ {IPC, Network, FileFormat, Plugin, Serialization, API}:
    
    recommended_signatures_match(T, U) = true
    ⟹ Safety(S) 在数据布局维度成立

**证明：**
- 对于 Layout 为主的场景 (IPC, Network, FileFormat-字节, Plugin-ABI)：
  签名匹配 ⟹ L_P(T) = L_P(U) [Theorem 3.1] ⟹ T ≅_mem U [Def 1.9] ⟹ Safety 成立 [Thm 4.1]

- 对于 Definition 为主的场景 (Serialization, API, Plugin-ODR, FileFormat-语义)：
  签名匹配 ⟹ D_P(T) = D_P(U) [Theorem 3.2] ⟹ 结构完全一致 [Cor 3.2.1]
  且 ⟹ L_P(T) = L_P(U) [Theorem 5.4] ⟹ 字节布局也一致

**无一场景存在误报 (false positive)。** 签名匹配总是意味着（在其编码范围内的）安全。 ∎

**定理 8.2 (完备性边界)。** 两层签名系统对**数据布局和数据结构**维度的安全条件完备，
但对以下维度不完备（by design）：

1. 数据值语义（指针值、vptr 值）
2. 成员函数 / 虚函数表内容
3. 静态成员、嵌套类型定义
4. 外部 IDL schema（protobuf field_number）
5. 字节序转换操作
6. 类型自身名称（结构分析设计选择）
7. access specifier（private/public/protected）

这些不完备项均为**有意的设计选择**，不是遗漏：
- 项 1-3, 7：超出 P2996 `nonstatic_data_members_of` 的反射范围
- 项 4：超出 C++ 类型系统的范围
- 项 5：职责分离（验证 ≠ 转换）
- 项 6：结构分析 vs 名义分析的设计决策 ∎
