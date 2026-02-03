# Change: Analyze Constexpr Step Limits

## Why

在复杂用例测试中，发现TypeLayout在处理以下场景时会超出编译器的constexpr评估步数限制：

1. **100+字段结构体** - Large100结构体编译失败
2. **10+类型std::variant** - Variant10编译失败

错误信息：`constexpr evaluation hit maximum step limit; possible infinite loop?`

这是编译器实现的约束，而非TypeLayout库本身的bug。需要分析：
- 根本原因
- 是否可以优化库实现减少步数消耗
- 是否应该在文档中明确限制
- 是否需要提供编译器参数建议

## What Changes

- 分析constexpr步数消耗的根本原因
- 测试不同`-fconstexpr-steps`值对限制的影响
- 优化TypeLayout实现（如果可行）以减少步数消耗
- 更新文档明确说明已知限制

## Impact

- Affected specs: `specs/signature`
- Affected code: `include/boost/typelayout/core/reflection_helpers.hpp`, `include/boost/typelayout/core/type_signature.hpp`
- Documentation: 需要更新使用指南和限制说明

---

## Analysis Results (分析结果)

### 根本原因

1. **CompileString 字符串操作**: 每次 `operator+` 调用都创建新数组并逐字符复制
2. **O(n²) 复制开销**: fold expression 左折叠导致 n + (n-1) + (n-2) + ... + 1 次复制
3. **签名字符串过长**: 100字段结构体签名长度约5200字符

### 已验证边界

| 测试用例 | 签名长度 | 所需最小步数 |
|----------|---------|-------------|
| Large60 | ~3100 chars | ~1M (默认足够) |
| Large100 | 5183 chars | **~2.19M** |
| Variant10 | 5079 chars | ~2.19M |

### 解决方案

**短期方案 (已实施)**:
- 在 `openspec/project.md` 添加 "Known Limitations" 文档
- 提供 `-fconstexpr-steps` 编译器参数建议
- 不修改库代码，作为已知限制记录

**长期优化 (待后续proposal)**:
- 考虑二分连接策略减少复制次数 (O(n²) → O(n log n))
- 考虑紧凑签名格式减少字符串长度
- 考虑哈希-only模式跳过字符串生成

### 推荐配置

| 项目类型 | 最大结构体 | 建议步数 |
|----------|-----------|---------|
| 小型项目 | <50 字段 | 默认 |
| 中型项目 | 50-80 字段 | 1.5M |
| 大型项目 | 80-100 字段 | **3M** |
| 超大型项目 | 100+ 字段 | 5M |
