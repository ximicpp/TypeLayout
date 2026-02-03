# Change: Analyze Alignment Completeness

## Why

内存对齐是类型布局的关键组成部分，需要验证 TypeLayout 是否完整捕获所有对齐信息：

1. **基本对齐** - 每个类型的 `alignof` 是否正确反映？
2. **字段对齐** - 结构体内字段的对齐是否可从签名推导？
3. **用户指定对齐** - `alignas` 属性是否正确捕获？
4. **对齐填充** - padding 是否在签名中体现？
5. **平台差异** - 不同平台的对齐差异是否正确区分？

## What Changes

这是一个**分析性 proposal**，验证对齐信息的完整性：

- 分析当前签名格式中的对齐信息
- 验证对齐信息是否足以重建布局
- 检测任何对齐信息的遗漏
- 评估是否需要增强对齐表示

## Impact

- Affected specs: signature/spec.md (可能需要澄清对齐语义)
- Core guarantee: 验证 "Identical signature ⟺ Identical layout" 是否包含对齐
- Documentation: 更新对齐相关文档
