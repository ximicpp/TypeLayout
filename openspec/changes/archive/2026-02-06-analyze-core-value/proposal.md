# Change: Core Value Analysis and Documentation Clarification

## Why

当前文档和 API 注释中多处使用"内存布局"(memory layout) 来描述签名编码的内容，但实际签名编码的是**类型 ABI 身份**——一个比裸字节布局更严格、包含结构语义的概念。这种表述不一致可能导致用户误解核心保证的边界。

具体问题：
1. 核心保证是**单向的**（签名匹配 → 布局兼容），但文档暗示双向
2. 继承类型与 flat struct 即使字节布局相同，签名也不同（这是正确行为，但未解释）
3. `polymorphic`/`inherited` 标记的设计理由未被文档化

## What Changes

### 文档更新
- 更新 README 核心保证表述，明确"ABI 身份"概念
- 添加设计决策文档，解释 `struct`/`class` 区分、继承标记的理由
- 添加"保证边界"章节，明确说明签名不匹配不一定意味着布局不同

### 代码注释
- 在 `type_signature.hpp` 添加设计决策注释
- 在 `reflection_helpers.hpp` 添加继承处理理由注释

## Impact

- Affected specs: `documentation`, `signature`
- Affected code: `README.md`, `type_signature.hpp`, `reflection_helpers.hpp`
- **BREAKING**: None — this is documentation/clarification only
