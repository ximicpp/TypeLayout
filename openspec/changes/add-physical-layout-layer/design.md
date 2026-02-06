# Design: Physical Layout Layer

## Context

TypeLayout 当前的 `Structural` 签名编码了完整的 C++ 对象模型信息，包括继承层次、多态标记等。这对于 ABI 兼容性验证是正确的，但在某些场景下过于严格：

- **C/C++ 互操作**：C 结构体 vs C++ 继承结构体，布局相同但签名不同
- **序列化/反序列化**：只关心字节排列，不关心如何声明
- **共享内存**：跨进程数据交换，类型定义可能不同但布局一致

## Goals

1. 提供 `Physical` 签名模式，只编码纯字节布局信息
2. 保证：布局相同的类型 → Physical 签名相同
3. 保持完全向后兼容，现有 API 行为不变

## Non-Goals

1. 不解决 `trivially_copyable` 检测问题
2. 不解决调用约定兼容性问题
3. 不处理虚基类的完美扁平化（v1 限制）

## Decisions

### Decision 1: 使用 `record` 统一前缀

**选择**: Physical 模式使用 `record` 前缀，不区分 `struct`/`class`

**理由**:
- `struct` vs `class` 在 C++ 中只影响默认访问权限，不影响布局
- 使用统一前缀强调 Physical 层只关心字节排列
- 避免跨层签名比较的混淆（Physical 和 Structural 永远不等）

**替代方案**: 保留 `struct`/`class` 区分
- 缺点：无法实现 "相同布局 = 相同签名" 的目标

### Decision 2: 继承扁平化策略

**选择**: 递归展开非虚基类，将所有字段以绝对偏移形式并列输出

**算法**:
```
physical_all_prefixed<T, OffsetAdj>:
    result = ""
    for each base B in bases_of(T):
        if B is non-virtual:
            result += physical_all_prefixed<B, OffsetAdj + offset_of(B)>
    for each member M in nonstatic_data_members_of(T):
        result += "," + field_signature(M, OffsetAdj)
    return result
```

**输出格式**: 每个字段以逗号前缀，最后由顶层函数 `skip_first()` 去除开头逗号

**理由**:
- 递归收集保证所有层级的字段都被包含
- 绝对偏移调整确保偏移值准确
- 逗号前缀策略简化空基类的处理

### Decision 3: 虚基类处理 (v1)

**选择**: Physical 模式递归时跳过虚基类

**理由**:
- 虚基类偏移依赖最终派生类型，中间基类无法可靠计算
- 虚继承在数据交换场景极少使用
- 类型的 `sizeof`/`alignof` 仍正确反映虚基类

**未来改进**: 可通过顶层收集所有虚基类（它们的偏移可从最终类型获取）

### Decision 4: vtable 指针处理

**选择**: Physical 模式不显式编码 vtable 指针

**理由**:
- vtable 指针不是用户数据，无法安全操作
- 通过字段偏移隐式体现（多态类型首个字段偏移 > 0）
- P2996 的 `nonstatic_data_members_of` 不返回 vtable 指针

**替代方案**: 显式编码为 `@0:ptr[s:8,a:8]`
- 缺点：需要特殊检测逻辑，增加复杂度
- 缺点：多态类型与首字段为指针的非多态类型可能混淆

### Decision 5: 字节数组归一化

**选择**: Physical 和 Structural 都归一化 `char`/`unsigned char`/`std::byte` 数组为 `bytes[]`

**理由**: 归一化是为了布局等价性，两层都需要

### Decision 6: API 设计

**选择**: 添加独立的 Physical API 而非修改现有 API

```cpp
get_physical_signature<T>()           // 新增
get_layout_signature<T>()             // 不变，默认 Structural
physical_signatures_match<T1, T2>()   // 新增
signatures_match<T1, T2>()            // 不变，默认 Structural
```

**理由**: 最大化向后兼容性，用户显式选择 Physical 层

## Risks / Trade-offs

### Risk 1: 虚基类字段丢失

**描述**: Physical 签名可能遗漏虚基类中的字段

**缓解**: 
- 明确文档记录此限制
- 类型的 size/alignment 仍正确
- 虚继承极少用于数据交换类型

### Risk 2: 签名长度增加

**描述**: 扁平化可能导致深层继承的签名更长

**缓解**: 
- 实际上签名变短（去掉了 `~base:` 嵌套）
- 哈希比较性能不受影响

### Risk 3: 用户误用

**描述**: 用户可能错误地认为 Physical 匹配 = memcpy 安全

**缓解**: 文档清晰说明 Physical 只保证布局匹配，不保证类型语义兼容

## Implementation Notes

### CompileString::skip_first()

```cpp
consteval auto skip_first() const noexcept {
    char result[N] = {};
    if (value[0] != '\0') {
        for (size_t i = 1; i < N; ++i) {
            result[i - 1] = value[i];
        }
    }
    return CompileString<N>(result);
}
```

### 位域偏移调整

位域的偏移由 `offset_of(member)` 返回 `{bytes, bits}`，调整时只加 `OffsetAdj` 到 `bytes`：

```cpp
constexpr std::size_t byte_offset = bit_off.bytes + OffsetAdj;
constexpr std::size_t bit_offset = bit_off.bits;  // 不变
```

### 测试验证用例

```cpp
struct Base { int x; };
struct Derived : Base { double y; };
struct Flat { int x; double y; };

static_assert(physical_signatures_match<Derived, Flat>());
static_assert(!signatures_match<Derived, Flat>());
```

## Open Questions

1. **是否需要 `PhysicalAnnotated` 模式？** — Physical + 字段名的组合，v1 暂不实现
2. **如何处理 `[[no_unique_address]]`？** — 遵循编译器布局，偏移正确即可
3. **是否需要排序字段？** — 当前按声明顺序，与内存顺序一致
