# Change: Update Technical Documentation

## Why

近期对 TypeLayout 进行了重大简化（Nano Architecture），需要更新技术文档以保持一致：

1. **移除了 Layer 2 (Serialization)**：不再有 `is_serializable<T>`、`PlatformSet` 等概念
2. **移除了 STL Opaque Specializations**：采用透明反射，直接反射 STL 类型的内部布局
3. **API 数量变化**：实际核心 API 从 "6 functions + 1 macro + 4 concepts" 调整为 "7 functions + 1 macro + 5 concepts"

## What Changes

### 文件影响

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| `doc/technical-report.md` | 修改 | 更新 API 描述、移除序列化相关内容、更新 STL 支持描述 |
| `doc/design-rationale.md` | 修改 | 更新 API 数量、STL 类型支持策略说明 |

### 具体变更点

#### technical-report.md
1. **§3.1 Library Design Philosophy**
   - 更新 API 列表（添加 `verifications_match<T, U>()`）
   - 更新 API 数量描述

2. **§3.2 Type Coverage Deep Dive**
   - 修改 STL 类型支持描述：从 "专用特化" 改为 "透明反射"
   - 移除 `std::atomic` 提及（已不支持）

3. **Future section**
   - 移除序列化功能作为 "what's next" 的提及
   - 调整为更准确的未来方向

#### design-rationale.md
1. **§3.1 Minimal Public API**
   - 更新函数数量和概念数量
   - 添加遗漏的 API（`hashes_match`, `verifications_match`, `get_layout_signature_cstr`）

2. **§4.1 STL Types**
   - 更新策略：从 "实现感知特化" 改为 "透明反射"
   - 说明现在直接反射 STL 内部成员

3. **§6.2 Potential Extensions**
   - 更新为实际的未来考虑方向

## Impact

- **文档准确性**：技术文档与实际实现对齐
- **用户理解**：避免用户对不存在功能的误解
- **技术报告可用性**：可直接用于会议提交
