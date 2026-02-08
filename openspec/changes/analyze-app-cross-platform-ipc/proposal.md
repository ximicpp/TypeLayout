# Change: Analyze Application — Cross-Platform Shared Memory / IPC

## Why
共享内存 / IPC 是 TypeLayout 最核心的应用场景之一。需要端到端分析此场景下
TypeLayout 的完整工作流、独特价值、与传统方案的对比、以及局限性。

## What Changes
- 分析 IPC/共享内存场景的典型痛点
- 展示 TypeLayout 的端到端工作流 (从类型定义到零拷贝传输)
- 对比传统方案 (手写序列化、boost::interprocess、protobuf IPC)
- 量化收益 (减少代码行数、消除运行时序列化开销)
- 识别局限 (动态大小数据、版本化、进程间指针)
- 将分析结论记录到 documentation spec

## Impact
- Affected specs: documentation
- Affected code: 可能新增 IPC 场景的示例
