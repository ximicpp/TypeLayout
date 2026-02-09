# Change: IPC/共享内存场景与两层签名的关系分析

## Why
IPC（进程间通信）和共享内存是 TypeLayout 最核心的应用场景之一。两个进程通过 mmap 或 POSIX 共享内存直接共享内存区域时，数据结构的字节布局必须完全一致，否则会导致数据损坏。需要系统性分析该场景中 Layout 层与 Definition 层各自的作用、选择理由，以及形式化正确性保证。

## What Changes
- 新增 IPC/共享内存场景的深度分析文档，作为 PROOFS.md 的应用场景附录
- 分析内容包括：
  - 为什么 IPC 场景主要使用 Layout 层（V1）
  - Definition 层（V2）在 IPC 场景中的辅助价值
  - 形式化保证链：Theorem 3.2 如何确保 memcpy 安全
  - 与传统方法（手动 static_assert sizeof/alignof）的对比
  - 边界条件分析（padding、endianness、pointer size）

## Impact
- Affected specs: signature (新增应用场景分析 requirement)
- Affected code: 无代码变更，纯分析文档
