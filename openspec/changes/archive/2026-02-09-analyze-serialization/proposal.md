# Change: 序列化版本检查场景与两层签名的关系分析

## Why
序列化框架（JSON、Protocol Buffers、自定义二进制序列化）在反序列化时需要确保数据结构与写入时一致。当结构体演化（字段增删、重命名、类型变更）时，序列化逻辑必须感知这些变化。需要分析 TypeLayout Definition 层为什么是序列化版本检查的主力，以及 Layout 层的辅助角色。

## What Changes
- 新增序列化版本检查场景的深度分析文档
- 分析内容包括：
  - 为什么序列化场景主要使用 Definition 层（V2）
  - Definition 层如何检测字段名变更和类型重构
  - Layout 层在序列化场景中的辅助角色（检测 padding 和对齐变化）
  - 与 protobuf field_number / JSON schema versioning 的对比
  - 典型案例：ConfigV1 → ConfigV2 的版本演化检测

## Impact
- Affected specs: signature
- Affected code: 无代码变更
