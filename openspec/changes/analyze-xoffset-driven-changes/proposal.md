# Change: 分析 XOffsetDataStructure 应用驱动的最新代码修改

## Why
最近一次提交 (`1df5cdd`) 新增了 `core/opaque.hpp`（3 个 opaque 宏）和
`signature_detail.hpp` 中的 `is_fixed_enum<T>()` API。这些修改的动机来自
XOffsetDataStructure（共享内存偏移容器库）的实际应用场景：当 TypeLayout 需要
为内部布局不透明的共享内存容器（如 XVector、XMap、XString）生成签名时，无法
通过 P2996 反射获取其内部成员，因此需要 opaque 注册机制。同时，enum 的跨进程
安全传输需要判断其是否有固定底层类型。

本 proposal 分析这些修改的技术合理性、与现有架构的一致性、以及潜在问题。

## What Changes
- 分析 `TYPELAYOUT_OPAQUE_TYPE` / `TYPELAYOUT_OPAQUE_CONTAINER` / `TYPELAYOUT_OPAQUE_MAP` 三个宏的设计
- 分析 `is_fixed_enum<T>()` 的正确性和完整性
- 评估与现有 signature spec 的一致性
- 识别缺口和改进建议

## Impact
- Affected specs: signature
- 影响对外 API 和类型覆盖范围