## Context

在 `audit-layout-signature` 审计中发现，P2996 反射的 `identifier_of()` 函数对匿名成员会抛出编译错误。
这影响了 `std::optional`、`std::variant` 以及用户定义的包含匿名联合的类型。

### 约束条件
- 必须在编译时检测匿名成员
- 解决方案必须与 Bloomberg Clang P2996 fork 兼容
- 不能破坏现有的签名格式
- 签名必须保持稳定性和唯一性

## Goals / Non-Goals

### Goals
- 支持所有包含匿名成员的类型生成布局签名
- 为匿名成员提供稳定、唯一的占位标识
- 保持向后兼容性

### Non-Goals
- 不尝试"命名"匿名成员（使用占位符即可）
- 不修改 P2996 API 的行为
- 不为匿名成员提供特殊的序列化语义

## Decisions

### Decision 1: 匿名成员检测方法

**选择**: 使用 `has_identifier()` 或尝试-捕获模式

**原因**:
P2996 提案提供了 `has_identifier(meta)` 来检查反射实体是否有标识符。
如果此 API 不可用，可以使用 `if constexpr` 结合 `requires` 表达式来安全检测。

```cpp
template<auto member>
consteval bool is_anonymous_member() {
    using namespace std::meta;
    // 方案 A: 使用 has_identifier (如果可用)
    return !has_identifier(member);
    
    // 方案 B: 使用 requires 表达式 (备选)
    // return !requires { identifier_of(member); };
}
```

### Decision 2: 占位名称格式

**选择**: `<anon:N>` 格式

**原因**:
| 格式 | 优点 | 缺点 |
|------|------|------|
| `<anon:N>` | 清晰可读、不与用户名冲突 | 稍长 |
| `$N` | 简短 | 可能与编译器符号冲突 |
| `_N` | 简短 | 可能与用户名冲突 |

`<anon:N>` 中的 `<>` 不是合法的 C++ 标识符字符，
因此保证不会与任何用户定义的成员名冲突。

### Decision 3: 索引策略

**选择**: 使用成员在类型中的全局索引

**原因**:
- 稳定性: 索引基于成员声明顺序，不会因编译器版本改变
- 唯一性: 每个成员有唯一索引
- 简单性: 直接使用 `Index` 模板参数

```cpp
template<typename T, std::size_t Index>
consteval auto get_member_name() {
    if constexpr (is_anonymous_member<member>()) {
        return CompileString{"<anon:"} + 
               CompileString<16>::from_number(Index) +
               CompileString{">"};
    } else {
        return CompileString<NameLen>(identifier_of(member));
    }
}
```

## Alternatives Considered

### Alternative 1: 跳过匿名成员
**拒绝原因**: 会丢失布局信息，导致签名不完整

### Alternative 2: 展开匿名联合的所有成员
**拒绝原因**: 会改变签名结构，破坏与实际内存布局的对应关系

### Alternative 3: 使用类型哈希作为名称
**拒绝原因**: 可读性差，且哈希可能冲突

## Risks / Trade-offs

### Risk 1: P2996 API 变化
- **风险**: `has_identifier()` 在最终标准中可能有不同名称或行为
- **缓解**: 使用条件编译和 fallback 实现

### Risk 2: 签名长度增加
- **风险**: `<anon:N>` 比短字段名长
- **缓解**: 可接受，因为清晰性更重要

## Migration Plan

1. 实现检测逻辑并验证
2. 修改 `get_field_signature()` 
3. 启用测试并验证
4. 更新文档

无需数据迁移，变更完全向后兼容。

## Open Questions

1. **Q**: Bloomberg Clang P2996 fork 是否支持 `has_identifier()`?
   - 需要在 Docker 容器中测试验证

2. **Q**: 匿名联合内部的成员是否需要递归处理?
   - 当前设计：将匿名联合作为整体处理，内部成员是联合自身的签名一部分
