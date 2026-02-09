# Change: 基于 ZST 模型更新跨平台兼容性用例代码

## Why

`analyze-zero-serialization-transfer` 建立了 ZST 充要条件 **C1 ∧ C2**，
`update-compat-with-zst-model` 更新了 spec 和 `compat_check.hpp` 的 summary 输出。

但 **用例代码**（`example/` 目录）仍然使用旧的理解模型，存在五个具体问题：

1. **`compat_check.cpp` static_assert 只验证 C1**：
   `UnsafeWithPointer` 通过了 layout_match，但含 `ptr[` 字段 → Safety=Warning → C2 不满足。
   代码注释 "Pointer types - same on both" 暗示可安全使用，这是**误导性的**。

2. **README 示例输出把 UnsafeWithPointer 标记为 "Serialization-free"**：
   按 ZST 模型，含指针的类型应显示 "Layout OK (pointer values not portable)"。

3. **README summary 使用旧的单一计数格式**：
   `87% (7/8) are serialization-free` 包含了含指针类型，与严格的 C1 ∧ C2 定义不符。

4. **compat_check.cpp 缺少 C2 维度的注释和说明**：
   用户可能误以为 "layout match = 零序列化安全"。

5. **README "Why This Matters" 节未提及安全分级条件**。

## What Changes

- 更新 `example/compat_check.cpp`:
  - 增加 C1/C2 维度注释
  - 区分 "Layout-only match" 和 "Serialization-free" 的 static_assert 说明
- 更新 `example/README.md`:
  - 示例输出反映新的四种 verdict 和双计数 summary
  - "Why This Matters" 节增加安全分级说明
  - 增加 ZST 条件速查参考
- 无 spec 变更（spec 已在上一个 proposal 中更新）

## Impact

- Affected specs: `cross-platform-compat` (delta: 补充用例场景)
- Affected code: `example/compat_check.cpp`, `example/README.md`
- 无 **BREAKING** 变更（仅文档和注释更新）
