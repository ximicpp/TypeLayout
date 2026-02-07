## Context

TypeLayout 当前有三种签名模式（Physical/Structural/Annotated），缺乏清晰的理论基础。
本次重构建立在"投影关系"这一数学模型上，将系统简化为两层：Layout（布局）和 Definition（定义）。

### 利益相关者
- 库用户（API 消费者）
- 库维护者
- 潜在的 Boost 审查者

### 约束
- 必须保持 header-only
- 必须保持 consteval/编译时计算
- 必须在 Bloomberg Clang P2996 上编译通过
- P2996 API 限制：`nonstatic_data_members_of` 不返回编译器生成的字段（vtable 指针等）

## Goals / Non-Goals

### Goals
- 建立两层签名的严格数学关系（投影）
- 清晰分离"字节兼容"和"定义一致"两个概念
- 统一签名前缀为 `record`（消除 struct/class 语义混淆）
- 为每个场景提供正确的签名层：共享内存用 Layout、插件系统用 Definition
- 使 Definition 签名成为一等公民，而非 Annotated 的"调试附属"

### Non-Goals
- 不支持外层类型名嵌入（设计决策：已确认不含）
- 不支持跨编译器的定义签名比较（名字表示可能不同）
- 不实现虚表指针的显式 `[__vptr]` 字段（P2996 不可见）
- 不提供旧 API 的 `[[deprecated]]` 过渡期（用户选择一步到位）

## Decisions

### Decision 1: 两层而非三层
- **决定**: 删除 Structural 模式，只保留 Layout 和 Definition
- **理由**: Structural（有结构无名字）是一个没有对应现实用例的中间态。所有"需要结构"的场景也需要名字（插件验证、ODR 检测、版本演化）；所有"不需要名字"的场景也不需要结构（共享内存、FFI）
- **替代方案**: 保留三层但重命名 → 增加认知负担，没有理论支撑

### Decision 2: 统一使用 `record` 前缀
- **决定**: Layout 和 Definition 签名都使用 `record[s:N,a:M]{...}` 前缀
- **理由**: C++ 的 `struct` vs `class` 仅是默认访问控制的区别，不影响内存布局。用 `record` 表示"一块有结构的内存"，语义更准确
- **替代方案**: 保留 struct/class 区分 → 引入语义混淆（struct 可以有虚函数，class 可以是 POD）

### Decision 3: Definition 签名不含外层类型名
- **决定**: `record[s:16,a:8]{...}` 而非 `record<Derived>[s:16,a:8]{...}`
- **理由**:
  - 外层类型名无判别价值（调用者已知类型）
  - 模板类型名长度爆炸且跨编译器不一致
  - 字段名和基类名已提供足够的语义信息
- **替代方案**: 含短名 `record<Config>` → 不同命名空间同名类型产生歧义

### Decision 4: 基类名使用短名
- **决定**: `~base<Base>` 使用 P2996 `identifier_of` 获取的短名
- **理由**: 在同一个类的上下文中，基类名通常不会歧义；即使歧义，字段内容也可区分
- **未来扩展**: 如果实践中发现歧义问题，可升级为限定名

### Decision 5: `polymorphic` 标记保留在 Definition 签名
- **决定**: Definition 签名中保留 `polymorphic` 标记，Layout 签名中不包含
- **理由**: 多态性影响类型语义（vtable 存在、unsafe cast 风险），属于定义层信息；布局层只关心字节偏移，vtable 指针的存在通过偏移间隙自然体现

### Decision 6: 虚表指针不显式列出
- **决定**: 不合成 `@0[__vptr]:ptr` 字段
- **理由**: P2996 的 `nonstatic_data_members_of` 不返回编译器生成的字段。虚表指针的空间通过第一个用户字段的偏移间隙和 `polymorphic` 标记隐含表达
- **影响**: Layout 签名中，多态类型的第一个字段偏移 > 0（通常 @8），但没有 @0 的字段 — 这准确反映了"偏移 0-7 被编译器占用但不是用户数据"

## Signature Format Specification

### Layout Signature 格式

```
[ARCH]record[s:SIZE,a:ALIGN]{@OFF1:TYPE1,@OFF2:TYPE2,...}
```

特征：
- 所有 class 类型使用 `record` 前缀
- 字段按绝对偏移排序，继承完全扁平化
- 无字段名、无基类边界、无 polymorphic/inherited 标记
- 虚基类跳过（v1 限制）
- 字节数组归一化为 `bytes[s:N,a:1]`

示例：
```
// struct Base { int id; }; struct Derived : Base { double value; };
[64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}
```

### Definition Signature 格式

```
[ARCH]record[s:SIZE,a:ALIGN,MARKERS]{BASES,@OFF[name]:TYPE,...}
```

特征：
- 所有 class 类型使用 `record` 前缀
- 字段保留名字 `[name]`
- 基类保留为子树 `~base<Name>:record[...]{...}`
- 虚基类使用 `~vbase<Name>:record[...]{...}`
- 多态类型标记 `polymorphic`
- 匿名成员使用 `<anon:N>` 占位符
- 字节数组归一化为 `bytes[s:N,a:1]`

示例：
```
// struct Base { int id; }; struct Derived : Base { double value; };
[64-le]record[s:16,a:8]{~base<Base>:record[s:4,a:4]{@0[id]:i32[s:4,a:4]},@8[value]:f64[s:8,a:8]}

// 多态类型
[64-le]record[s:24,a:8,polymorphic]{~base<Base>:record[s:16,a:8,polymorphic]{@8[id]:i32[s:4,a:4]},@16[value]:f64[s:8,a:8]}

// 虚继承菱形
[64-le]record[s:...,a:...]{~base<B>:record[...]{@...[y]:i32[...]},~base<C>:record[...]{@...[z]:i32[...]},~vbase<A>:record[...]{@...[x]:i32[...]},@...[w]:i32[...]}
```

### 投影关系

```python
def project(definition_sig):
    """将 Definition 签名投影为 Layout 签名"""
    fields = collect_leaf_fields(definition_sig)  # 递归收集叶子字段
    for field in fields:
        field.offset = absolute_offset(field)     # 计算相对于根的绝对偏移
        field.name = None                         # 丢弃名字
    fields.sort(by=offset)                        # 按偏移排序
    return flatten(fields)                        # 拼接为扁平签名
```

数学性质：
- `definition_match(T, U) ⟹ layout_match(T, U)` — 定义相同一定布局相同
- `layout_match(T, U) ⟹/ definition_match(T, U)` — 布局相同不一定定义相同

## Risks / Trade-offs

### Risk 1: 完全破坏性 API 变更
- **风险**: 所有现有用户代码立即失效
- **缓解**: 项目仍在 pre-1.0 阶段，用户基数极小；版本号升级至 2.0.0

### Risk 2: Definition 签名长度增加
- **风险**: 相比旧 Structural 模式，Definition 签名包含名字，长度增加约 50-80%
- **缓解**: 大部分场景使用 Layout 签名（更短）；Definition 签名按需使用

### Risk 3: 基类短名歧义
- **风险**: 不同命名空间的同名基类在 Definition 签名中无法区分
- **缓解**: 极端边缘情况；字段内容差异会导致签名不同；未来可升级为限定名

## Open Questions

- Definition 签名中是否需要 `inherited` 标记？（建议：不需要，`~base<>` 的存在已隐含继承）
- 是否需要为非类型（基本类型、指针、数组等）也区分两层？（建议：不需要，基本类型两层相同）
