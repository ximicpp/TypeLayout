# Change: Analyze Application — Network Protocol Wire-Format Verification

## Why
网络协议开发中，确保客户端和服务端对 wire-format 的理解一致是关键挑战。
TypeLayout 可以在编译时证明协议头部结构的跨平台一致性，消除 wire-format 不匹配导致的隐蔽 bug。

## What Changes
- 分析网络协议 wire-format 验证的典型痛点
- 展示 TypeLayout 在协议头验证中的工作流
- 对比传统方案 (手动 offsetof/sizeof, protobuf, FlatBuffers, Cap'n Proto)
- 分析字节序处理的边界 (TypeLayout 检测但不转换)
- 识别局限 (变长字段、TLV 编码、字节序转换)

## Impact
- Affected specs: documentation
- Affected code: 无代码变更，纯分析性质
