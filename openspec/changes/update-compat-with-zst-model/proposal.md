# Change: 基于 ZST 分析模型更新跨平台兼容性用例

## Why

`analyze-zero-serialization-transfer` 的分析建立了 ZST（零序列化传输）的精简充要条件：
**C1（布局签名匹配）∧ C2（安全分类 = Safe）**。

但当前的跨平台兼容性规格和实现存在三个与此模型不一致之处：

1. **`cross-platform-compat` spec 的 verdict 描述不完整**：
   仅提到 "Serialization-free" 和 "Needs serialization" 两种判定，
   但实际实现有四种 verdict（对应 C1×C2 的四种组合），spec 未体现 C2 维度的区分。

2. **`signature` spec 的 Usage Guidance 缺少 ZST 前置条件**：
   Use Case 表仅推荐使用的签名层，未说明零序列化还需要满足 C2（Safety Classification）条件。
   用户可能误以为"Layout MATCH = 可以零序列化"，忽略了指针/位域的额外风险。

3. **`CompatReporter` 的 compatible 计数逻辑**：
   代码中 `if (r.layout_match) ++compatible` 只考虑 C1 不考虑 C2，
   导致 "75% compatible" 统计中可能包含 Warning/Risk 类型，
   与 ZST 的严格定义不一致。

## What Changes

- **MODIFIED** `cross-platform-compat` spec: 更新 Runtime Compatibility Report 的 verdict 描述，
  明确四种 verdict 及其与 C1/C2 的映射关系
- **MODIFIED** `signature` spec: 更新 Usage Guidance 表，增加 ZST 条件提示
- **代码变更**: 调整 `CompatReporter::print_report()` 的 compatible 计数逻辑，
  区分 "serialization-free" 和 "layout-compatible" 两个统计维度

## Impact

- Affected specs: `signature`, `cross-platform-compat`
- Affected code: `include/boost/typelayout/tools/compat_check.hpp` (print_report 统计逻辑)
- **BREAKING**: CompatReporter 输出格式微调（summary 统计数字可能变小，因为更严格）
