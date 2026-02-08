## Context

共享内存 / IPC 是 TypeLayout 最直接的应用场景。两个进程通过 mmap 共享同一块内存区域，
需要确保双方对内存中结构体的理解完全一致。传统方式依赖人工保证或运行时序列化，
TypeLayout 提供了编译时证明。

## Goals / Non-Goals

- Goals:
  - 论证 TypeLayout 在 IPC 场景的独特价值
  - 量化与传统方案的差异
  - 识别适用边界

- Non-Goals:
  - 不实现完整的 IPC 框架
  - 不替代 boost::interprocess

## Decisions

### 场景描述

**典型 IPC 共享内存工作流:**
```
进程 A (Writer)                          进程 B (Reader)
┌──────────────────┐                    ┌──────────────────┐
│ struct ShmHeader  │                    │ struct ShmHeader  │
│ {                 │     mmap/shm      │ {                 │
│   uint32_t magic; │ ◄──────────────► │   uint32_t magic; │
│   uint64_t seq;   │                    │   uint64_t seq;   │
│   ...             │                    │   ...             │
│ };                │                    │ };                │
└──────────────────┘                    └──────────────────┘
     编译器 A                                编译器 B
```

**痛点:** 进程 A 和进程 B 可能使用不同编译器、不同平台。如果 `ShmHeader` 的
内存布局不一致，读取的数据就是垃圾值。

### TypeLayout 的端到端工作流

```
1. 定义共享类型 (shared_types.hpp)
   struct ShmHeader { uint32_t magic; uint64_t seq_num; uint32_t flags; };

2. Phase 1: 在每个目标平台导出签名
   平台 A → sigs/x86_64_linux_clang.sig.hpp
   平台 B → sigs/arm64_linux_clang.sig.hpp

3. Phase 2: 编译时验证 (任何 C++17 编译器)
   TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, arm64_linux_clang)
   // 编译通过 = 零拷贝安全 ✅
   // 编译失败 = 需要序列化 ❌

4. 运行时: 直接 memcpy / mmap，无需序列化
   auto* header = static_cast<ShmHeader*>(mmap_ptr);
   header->magic = 0xDEADBEEF;  // 直接写入，零开销
```

### 与传统方案对比

| 维度 | 手写序列化 | boost::interprocess | protobuf + shm | TypeLayout |
|------|-----------|--------------------|--------------|-----------||
| 验证时机 | 运行时 (或无) | 运行时 | 编译时 (schema) | **编译时** |
| 运行时开销 | 序列化/反序列化 | 有 (managed_shm) | 编解码 | **零** |
| 代码量 | 高 (手写序列化函数) | 中 | 中 (.proto + 生成代码) | **极低** (3 行宏) |
| 跨平台安全 | 人工保证 ❌ | 库保证 ✅ | Schema 保证 ✅ | **编译时证明** ✅ |
| 类型注解 | 无 | allocator 侵入 | .proto 定义 | **零注解** |
| 动态数据 | 自行处理 | 支持 | 支持 | ❌ 不支持 |
| 版本化 | 自行处理 | 不支持 | 完善支持 | 签名快照可比较 |

### 量化收益

**传统方式 (手写序列化):**
```cpp
// 每个类型需要 ~20 行序列化代码
void serialize(const ShmHeader& h, uint8_t* buf) {
    memcpy(buf, &h.magic, sizeof(h.magic)); buf += 4;
    memcpy(buf, &h.seq_num, sizeof(h.seq_num)); buf += 8;
    memcpy(buf, &h.flags, sizeof(h.flags)); buf += 4;
}
void deserialize(ShmHeader& h, const uint8_t* buf) { ... }
// 还需要字节序转换、对齐处理等
```

**TypeLayout 方式:**
```cpp
// 3 行 — 编译时证明后直接 memcpy
TYPELAYOUT_ASSERT_COMPAT(platform_a, platform_b)
auto* header = static_cast<ShmHeader*>(shm_ptr);  // 直接用
```

**节省: 每个类型减少 ~20 行序列化代码 + 消除运行时编解码开销。**

### 适用边界

**TypeLayout 适用:**
- 固定大小结构体 (所有字段在编译时确定大小)
- 同一类型定义在双方编译 (通过共享头文件)
- 不含进程间指针 (或指针仅用于进程内)

**TypeLayout 不适用:**
- 包含 `std::string`、`std::vector` 等动态大小字段
- 需要向后兼容新旧版本的结构体 (需要版本化 schema)
- 需要跨语言 IPC (Python ↔ C++)

**互补使用模式:**
```
固定头部 (TypeLayout 验证) + 动态 payload (protobuf 序列化)
┌────────────────────┬─────────────────────────────────┐
│ ShmHeader (固定)   │ protobuf payload (变长)          │
│ TypeLayout ✅       │ protobuf ✅                      │
└────────────────────┴─────────────────────────────────┘
```

### TypeLayout 在此场景的独特价值

1. **零运行时开销**: 验证在编译时完成，运行时直接 `memcpy`
2. **零注解**: 不需要 `.proto` 文件、不需要 allocator 侵入、不需要序列化函数
3. **编译时证明**: `static_assert` 失败 = 编译不过，不存在"忘记检查"的风险
4. **人类可读诊断**: 签名字符串可直接读懂，快速定位布局差异

## Risks / Trade-offs

- 不支持动态大小数据 → 与 protobuf 互补使用
- 不支持版本化 → 签名快照可用于 diff，但无自动兼容性处理
- padding 字节未初始化 → 建议用户使用 `memset` 初始化整个共享内存区域

## Open Questions

- 是否提供 `typelayout::shm_cast<T>(void*)` 安全转换辅助函数？→ 低优先级
- 是否集成 POSIX shm_open / mmap 的示例？→ 可作为 example 补充
