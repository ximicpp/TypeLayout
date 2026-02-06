# Tasks: analyze-core-value

## 1. Core Value Analysis (已完成)
- [x] 1.1 分析核心签名机制
- [x] 1.2 识别保证的精确边界（单向性）
- [x] 1.3 识别设计张力（布局 vs 结构）
- [x] 1.4 验证代码实现与分析结论一致

## 2. Documentation Updates
- [x] 2.1 更新 README 核心保证表述 (已有 ABI 说明)
- [x] 2.2 创建 `doc/design/abi-identity.md` 设计决策文档
- [ ] 2.3 添加"保证边界"章节到用户指南 (deferred: 当前 README 已包含足够说明)

## 3. Code Comments
- [x] 3.1 在 `type_signature.hpp` struct/class 区分处添加设计注释
- [x] 3.2 在 `reflection_helpers.hpp` 基类处理处添加理由注释

## 4. Validation
- [x] 4.1 确保所有测试通过
- [x] 4.2 验证文档与代码一致