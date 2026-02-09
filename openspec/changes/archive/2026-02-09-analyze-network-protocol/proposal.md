# Change: 网络协议验证场景与两层签名的关系分析

## Why
网络协议的二进制头部结构（wire format）是跨平台通信的基石。客户端和服务端必须对报文头的字节布局达成一致，否则解析会失败。需要分析 TypeLayout 两层签名在网络协议验证中的具体角色，特别是 Layout 层如何保证 wire-format 一致性，以及 endianness 检测的边界。

## What Changes
- 新增网络协议验证场景的深度分析文档
- 分析内容包括：
  - 为什么网络协议主要使用 Layout 层
  - TypeLayout 的字节序检测能力及其边界（检测 ≠ 转换）
  - 与手动 offsetof/sizeof 断言的完备性对比
  - 示例：PacketHeader 结构的签名验证流程

## Impact
- Affected specs: signature
- Affected code: 无代码变更
