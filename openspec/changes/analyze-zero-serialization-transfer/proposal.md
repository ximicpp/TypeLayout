# Change: 零序列化传输的充要条件——从签名理论到网络传输实践

## Why

Boost.TypeLayout 的核心价值主张之一是：**当布局签名匹配时，可以跳过序列化直接传输**。
但当前文档缺乏一份系统性分析来回答以下关键问题：

1. **充要条件**：用户需要确保什么类型的签名一致，才能在集合内直接传输？
   布局签名匹配是充分条件还是充要条件？额外的前置条件（IEEE 754、字节序、无指针）是什么？

2. **类型分类学**：C++ 类型空间中，哪些类型天然满足零序列化条件？哪些通过改造可满足？
   哪些类型永远无法满足？需要一个系统的分类体系。

3. **网络传输实践**：从理论条件到 `send()/recv()` 的实际应用链路中，还有哪些环节
   需要验证？wire-format 对齐、TCP 分包、多类型消息等实际问题如何处理？

已有的 `analyze-network-protocol` 和 `analyze-cross-platform-collection-compat` 分别
从场景维度和集合维度做了分析，但未系统回答"什么条件下不需要序列化"这一根本问题。

## What Changes

- 新增 `ZERO_SERIALIZATION_TRANSFER.md` 系统分析文档，包含：
  - §1: 零序列化传输的形式化定义与充要条件推导
  - §2: C++ 类型分类学（Safe/Conditional/Unsafe 三级分类体系）
  - §3: 充要条件的完整证明链（V1 + 编码忠实性 + 前置条件 → 零拷贝安全）
  - §4: 网络传输场景深挖（wire-format、字节序、TCP 流、消息协议）
  - §5: 从 Layout Match 到 send/recv 的完整安全链
  - §6: 类型改造指南（如何将 Unsafe 类型改造为 Safe 类型）
  - §7: 与序列化框架的决策树（何时用零拷贝、何时用 protobuf/FlatBuffers）
  - §8: 实战案例与代码模式
- 新增对 signature spec 的 delta（强化零序列化条件的形式化定义）
- 新增对 cross-platform-compat spec 的 delta（补充传输安全验证流程）

## Impact

- Affected specs: `signature`, `cross-platform-compat`
- Affected code: 无代码变更，纯分析文档
- Affected files: 新增 `docs/analysis/ZERO_SERIALIZATION_TRANSFER.md`
- 关联变更: 整合 `analyze-network-protocol` 和 `analyze-cross-platform-collection-compat` 的结论
