## Context

网络协议依赖精确的 wire-format 定义。客户端和服务端必须对每个字节的含义达成共识。
传统方式使用手动 `offsetof` / `sizeof` 检查或引入序列化框架。TypeLayout 提供了
编译时自动化的 wire-format 一致性证明。

## Goals / Non-Goals

- Goals:
  - 论证 TypeLayout 在网络协议场景的价值
  - 分析字节序边界 (检测 vs 转换)
  - 对比 protobuf/FlatBuffers/Cap'n Proto

- Non-Goals:
  - 不替代网络库 (如 Boost.Asio)
  - 不处理字节序转换

## Decisions

### 场景描述

**典型协议头定义:**
```cpp
// protocol.hpp — 客户端和服务端共享
struct PacketHeader {
    uint32_t magic;       // @0: 0xDEADBEEF
    uint16_t version;     // @4: 协议版本
    uint16_t type;        // @6: 消息类型
    uint32_t payload_len; // @8: payload 长度
    uint32_t checksum;    // @12: CRC32
};
// sizeof = 16, 无 padding — 完美 wire-format
```

**痛点:**
1. 不同平台编译器可能插入不同的 padding
2. 字段大小可能因 ABI 不同而变化 (如 `long` 在 LP64 vs LLP64)
3. 手动 `static_assert(offsetof(...) == N)` 容易遗漏或写错
4. 协议演进时检查需要同步更新

### TypeLayout 的工作流

```cpp
// Phase 1: 导出签名
TYPELAYOUT_EXPORT_TYPES(PacketHeader)
// 生成: "[64-le]record[s:16,a:4]{@0:u32[s:4,a:4],@4:u16[s:2,a:2],
//         @6:u16[s:2,a:2],@8:u32[s:4,a:4],@12:u32[s:4,a:4]}"

// Phase 2: 跨平台验证
TYPELAYOUT_ASSERT_COMPAT(client_platform, server_platform)
// 编译通过 = wire-format 一致 ✅
```

**与手动检查对比:**
```cpp
// 传统方式 — 每次修改协议都要手动更新
static_assert(sizeof(PacketHeader) == 16);
static_assert(offsetof(PacketHeader, magic) == 0);
static_assert(offsetof(PacketHeader, version) == 4);
static_assert(offsetof(PacketHeader, type) == 6);
static_assert(offsetof(PacketHeader, payload_len) == 8);
static_assert(offsetof(PacketHeader, checksum) == 12);
// 5 个 static_assert — 容易遗漏！

// TypeLayout 方式 — 自动覆盖所有字段
TYPELAYOUT_ASSERT_COMPAT(platform_a, platform_b)
// 1 行 — 永远不会遗漏任何字段
```

### 与序列化框架对比

| 维度 | 手动 offsetof | protobuf | FlatBuffers | Cap'n Proto | TypeLayout |
|------|-------------|----------|-------------|-------------|------------|
| Wire-format 控制 | 完全 | 无 (自定义格式) | 部分 | 部分 | **完全** |
| 验证自动化 | 手动 | 自动 (schema) | 自动 | 自动 | **自动** |
| 运行时开销 | 零 | 编解码 | 近零 | 零 | **零** |
| 使用原生 struct | ✅ | ❌ (生成类) | ❌ (Builder) | ❌ (生成类) | **✅** |
| 跨语言 | C/C++ | 多语言 | 多语言 | 多语言 | C++ |

**关键差异:** protobuf/FlatBuffers 接管了 wire-format (你不能控制字节如何排列)。
TypeLayout 让你保留原生 C++ struct，只是验证其布局。对于需要精确控制 wire-format 的协议
(如硬件协议、遗留系统兼容)，这是本质区别。

### 字节序边界

TypeLayout 在签名中编码了字节序 (`[64-le]` / `[32-be]`)：
- 如果两端字节序不同，签名会不匹配 → **主动检测** ✅
- 但 TypeLayout **不负责字节序转换** → 用户仍需 `htonl/ntohl`

**这是正确的职责分离:**
- TypeLayout: "这两个平台的布局是否一致？" (包含字节序)
- 网络库: "如何在不同字节序之间转换？"

### 典型网络协议类型的 TypeLayout 适用性

| 协议类型 | TypeLayout 适用性 | 原因 |
|---------|------------------|------|
| 固定头部 (Ethernet, IP, TCP) | ✅ 非常适合 | 所有字段固定宽度，精确控制偏移 |
| TLV 编码 (RADIUS, DHCP) | ⚠️ 仅头部 | 头部固定，TLV payload 动态 |
| 变长消息 (HTTP) | ❌ 不适用 | 文本协议，非二进制布局 |
| 自定义二进制协议 | ✅ 非常适合 | 用户完全控制 struct 定义 |

## Risks / Trade-offs

- 不处理字节序转换 → 明确职责边界
- 不支持变长字段 → 适用于固定格式头部

## Open Questions

- 是否应在报告中标注 endianness 不匹配作为特殊错误？→ 已标注在架构前缀中
- 是否应提供 `#pragma pack(1)` 的签名验证？→ 当前签名自然反映打包效果
