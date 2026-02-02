# Change: TypeLayout 兼容性分析与限制规范

## Why

在完备性分析中识别出三个需要深入分析的问题，经过分析后发现：

1. **虚拟继承** - 不是 TypeLayout 的问题，签名中包含偏移信息，签名比较自动处理兼容性
2. **位域** - 不是 TypeLayout 的问题，签名中包含位位置，签名比较自动处理兼容性
3. **std::variant/optional** - 真正的限制，因为有运行时状态（活动索引/engaged 标志）

核心认知：**签名相同 ⟺ 内存布局相同 ⟺ memcpy 安全**

## What Changes

### 核心结论

| 类型特征 | TypeLayout | 序列化 | 原因 |
|---------|-----------|--------|------|
| 虚拟继承 | ✅ 正常工作 | ✅ 允许 | 签名包含偏移信息 |
| 位域 | ✅ 正常工作 | ✅ 允许 | 签名包含位位置 |
| std::tuple | ✅ 正常工作 | ✅ 允许 | 签名反映实际布局 |
| std::variant | ✅ 正常工作 | ❌ 拒绝 | **运行时状态问题** |
| std::optional | ✅ 正常工作 | ❌ 拒绝 | **运行时状态问题** |

### 需要的改动

- 更新文档，说明签名驱动的兼容性模型
- 确认 `is_serializable_v` 正确拒绝 `std::variant` 和 `std::optional`
- 添加关于跨平台协议的最佳实践指南（特别是位域）

### 不需要改动

- ❌ 不需要特殊拒绝虚拟继承
- ❌ 不需要特殊拒绝位域
- ❌ 不需要为 std::tuple 添加专用特化

## Impact

- Affected specs: `specs/signature`, `specs/portability`
- Affected docs: `user-guide/type-support.adoc`, `design-rationale/`
- 无代码破坏性变更 - 主要是文档和分析更新