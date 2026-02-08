## Context

二进制文件格式 (如图像格式、数据库文件、日志文件) 依赖固定的头部结构。
写入端和读取端可能运行在不同平台、不同编译器版本上。TypeLayout 可以在编译时
验证头部结构的一致性，防止文件损坏或数据误读。

## Goals / Non-Goals

- Goals:
  - 论证 TypeLayout 在文件格式场景的价值
  - 分析版本演进检测能力
  - 展示 Layout + Definition 两层如何协同

- Non-Goals:
  - 不实现文件 I/O 库
  - 不处理变长记录或压缩

## Decisions

### 场景描述

**典型文件格式头:**
```cpp
struct FileHeader {
    char     magic[4];      // "MYFT"
    uint32_t version;       // 文件格式版本号
    uint64_t timestamp;     // 创建时间戳
    uint32_t entry_count;   // 记录数量
    uint32_t reserved;      // 保留字段
};
// sizeof = 24, 明确的字节布局
```

**痛点:**
1. **版本漂移**: 程序 V2 修改了 FileHeader 结构，但忘记更新 version 字段
2. **平台差异**: 在 32-bit 系统上读取 64-bit 系统写入的文件
3. **编译器差异**: 不同编译器对同一 struct 可能有不同的 padding 策略
4. **人工维护**: 文件格式规范文档与实际代码不同步

### TypeLayout 的价值

#### 跨平台验证 (V1 Layout)
```cpp
// 确保 Linux 写入的文件可以在 Windows 读取
TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, x86_64_windows_msvc)
// 编译通过 = 可以直接 fread 整个 FileHeader ✅
```

#### 版本演进检测 (V2 Definition)

这是文件格式场景中 Definition 签名的独特价值:

```cpp
// V1 版本
struct FileHeaderV1 {
    char     magic[4];
    uint32_t version;
    uint64_t timestamp;
    uint32_t entry_count;
    uint32_t reserved;
};

// V2 版本 — 字段名变更 + 新增字段
struct FileHeaderV2 {
    char     magic[4];
    uint32_t format_version;    // 重命名: version → format_version
    uint64_t created_at;        // 重命名: timestamp → created_at
    uint32_t record_count;      // 重命名: entry_count → record_count
    uint32_t flags;             // 变更: reserved → flags (语义变化!)
};

// Layout 签名: 相同 ✅ (字节布局未变)
// Definition 签名: 不同 ❌ (字段名全变了)

// 这正是 V2 的价值:
// - 如果只用 Layout 检查，会错误地认为 V1 和 V2 兼容
// - Definition 签名正确地检测到了结构变更
```

**两层协同使用:**
| 检查 | 结果 | 含义 |
|------|------|------|
| Layout MATCH + Definition MATCH | 完全兼容 | 零拷贝安全，字段语义一致 |
| Layout MATCH + Definition DIFFER | 布局兼容 | 可以读取数据，但字段含义可能已变 |
| Layout DIFFER | 不兼容 | 需要版本转换逻辑 |

### 签名作为"格式指纹"

一个独特的使用模式：将签名字符串嵌入文件头本身:

```cpp
struct SelfDescribingHeader {
    char     magic[4];
    uint32_t version;
    char     layout_sig[128];  // 写入时填入签名字符串
    uint64_t data_offset;
};

// 写入时:
// header.layout_sig = get_layout_signature<DataRecord>();
// 读取时: 比较文件中的签名与当前编译的签名
```

这实现了**运行时的跨版本兼容检测**——不需要手动管理 version 数字。

### 与传统方案对比

| 维度 | Magic + Version | 格式规范文档 | TypeLayout |
|------|----------------|-------------|------------|
| 检测时机 | 运行时 | 人工审查 | **编译时** |
| 检测精度 | 版本号粒度 | 依赖文档质量 | **字段级精度** |
| 维护成本 | 需同步 version 字段 | 需同步文档 | **自动化** |
| 字段级变更检测 | ❌ | 人工 | **✅ (V2 Definition)** |

### 适用性

| 文件类型 | TypeLayout 适用性 | 原因 |
|---------|------------------|------|
| 固定头部 + 固定记录 | ✅ 非常适合 | 全部固定大小 |
| 固定头部 + 变长记录 | ⚠️ 仅头部 | 头部可验证，记录需其他方式 |
| 全变长格式 (JSON, XML) | ❌ | 非二进制布局 |
| 数据库页面 (B-tree) | ✅ 页面头适合 | 页面头通常是固定结构 |

## Risks / Trade-offs

- 签名字符串可能很长 (>100 字符)，嵌入文件头占空间
  → 缓解: 可以只嵌入签名的哈希值
- 不支持变长记录的兼容检测
  → 缓解: 将变长部分交给序列化框架

## Open Questions

- 是否提供 `signature_hash()` 函数生成短哈希？→ 可考虑
- 是否提供文件格式的最佳实践示例？→ 可作为 example 补充
