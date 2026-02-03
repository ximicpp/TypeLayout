# Change: analyze-complex-cases

## Why

当前TypeLayout的测试覆盖了基础类型、简单嵌套和标准库容器，但缺乏对以下复杂场景的系统性验证：

1. **深层递归嵌套**：多层结构体嵌套（如5层、10层）是否能正确生成签名
2. **大型结构体**：包含大量成员（50+、100+字段）的结构体是否在编译时资源限制内
3. **复杂模板递归**：如`std::tuple`嵌套、CRTP模式、模板元编程结构
4. **边界情况**：空基类优化、菱形继承、变长模板参数

验证这些场景对于确保TypeLayout在实际生产环境中的可用性至关重要。

## What Changes

- 创建深层嵌套测试用例（5层、10层嵌套结构）
- 创建大型结构体测试用例（50字段、100字段）
- 创建复杂模板递归测试用例
- 测试编译时资源消耗和限制
- 验证签名的正确性和一致性
- 记录任何发现的限制或问题

## Impact

- Affected specs: `signature`
- Affected code: `test/` 目录
- 无破坏性变更 - 仅为验证性测试

## Test Categories

### Category 1: Deep Nesting (递归深度)
- Level 5 nested struct
- Level 10 nested struct
- Level 15+ nested struct (边界测试)

### Category 2: Large Structures (大型结构)
- 50 members struct
- 100 members struct
- 200+ members struct (边界测试)

### Category 3: Complex Templates (复杂模板)
- Deeply nested `std::tuple`
- CRTP patterns
- Variadic template recursion
- `std::variant` with many alternatives

### Category 4: Inheritance Complexity (继承复杂度)
- Diamond inheritance
- Deep virtual inheritance chain
- Multiple virtual bases

### Category 5: Compile-time Metrics (编译时指标)
- 编译时间测量
- 模板实例化深度
- consteval 资源消耗

## Success Criteria

1. 所有测试用例编译通过
2. 生成的签名与预期布局一致
3. 记录任何编译器限制（如模板深度限制）
4. 编译时间在可接受范围内
