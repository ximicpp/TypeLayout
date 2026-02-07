# TypeLayout 核心价值对齐分析报告

## 1. 核心价值声明

**project.md 声明**: `Identical signature ⟺ Identical memory layout`
**README 声明**: 两层签名系统，`definition_match(T,U) ⟹ layout_match(T,U)`

## 2. 核心保证的数学分析

### 2.1 Layout 签名双射性

**命题**: `layout_sig(T) == layout_sig(U) ⟺ memcmp-compatible(T, U)`

**正向** (sig 相同 → 布局相同): ✅ **成立**
- 签名编码了 `sizeof(T)`, `alignof(T)`, 每个叶子字段的 `@offset:type[s:N,a:M]`
- 继承和组合均被展平为绝对偏移量
- vptr 标记确保多态 vs 非多态不混淆
- 因此签名相同 → 所有字段在相同偏移有相同大小/类型 → 内存布局相同

**反向** (布局相同 → sig 相同): ⚠️ **部分成立，有例外**

| 情况 | 是否双射 | 说明 |
|------|---------|------|
| 基本类型 | ✅ | `int32_t` 到 `i32` 是确定性映射 |
| 平台类型 `long` | ✅ | 按 sizeof 规范化为 `i32`/`i64` |
| 枚举 | ✅ | Layout 只看底层类型 |
| 空 class | ⚠️ | `struct E1 {}` 和 `struct E2 {}` 签名相同 `record[s:1,a:1]{}`，但 C++ 保证不同类型有不同地址。对 memcpy 场景无害 |
| 含 padding 的 struct | ✅ | padding 被 size 和 offset 隐式捕获 |
| 数组 of struct | ⚠️ | `struct S {int a; int b;}[2]` — 数组不展平内部 struct，但 struct 本身会被展平。因此 `S[2]` 与另一个同元素类型的数组匹配 |
| union | ✅ | 全展平，以 size/align 和成员签名区分 |

**结论**: Layout 双射性在实用场景下成立。空类型的 "false positive" 对目标场景（memcpy/mmap）无害。

### 2.2 Definition 签名双射性

**命题**: `def_sig(T) == def_sig(U) ⟺ structural_identity(T, U)`

**正向**: ✅ **成立**
- 编码了字段名、字段类型、偏移量、基类限定名、多态标记
- 枚举包含限定名

**反向**: ⚠️ **有已知限制**

| 情况 | 是否双射 | 说明 |
|------|---------|------|
| 同名同布局的不同 namespace struct | ⚠️ | struct 本身不编码自身限定名，两个 `ns1::Foo{int x}` 和 `ns2::Foo{int x}` 的 Definition 签名相同。这是**设计决策**（结构恒等 vs 名义恒等） |
| 基类 | ✅ | 修复后使用限定名 |
| 枚举 | ✅ | 修复后使用限定名 |
| 模板实例 | ⚠️ | `vector<int>` 的签名取决于其内部字段布局，与 `vector<long>` 可能不同也可能相同（取决于平台 long 大小） |

### 2.3 投射关系

**命题**: `definition_match(T, U) ⟹ layout_match(T, U)`

**分析**: ⚠️ **在当前实现中不一定成立**

反例场景：Definition 层不展平组合，Layout 层展平组合。

```cpp
struct Inner { int a; int b; };
struct A { Inner x; double d; };  // Definition: {...@0[x]:record{...}...}
struct B { Inner x; double d; };  // Definition: {...@0[x]:record{...}...}
// definition_match(A, B) = true
// layout_match(A, B) = true  ← OK
```

但更关键的是：Definition 保留 `record[s:N,a:M]{...}` 包装，Layout 展平它。两者在计算签名时走**完全不同的代码路径**。投射关系要求：如果 Definition 签名逐字符相同，则 Layout 签名也逐字符相同。

由于 Definition 签名中的嵌套 `record{...}` 与 Layout 签名中的展平字段使用不同的格式，投射关系**不是通过格式推导成立的，而是通过语义推导**：相同的 Definition 签名 → 相同的字段类型和偏移 → 相同的展平结果 → 相同的 Layout 签名。

**结论**: 投射关系在**语义层面**成立，但没有形式化证明。建议在测试中添加更多验证。

## 3. 功能差距分析

### 3.1 project.md 声称 vs 实际实现

| 声称的 API/功能 | 实际状态 | 差距 |
|----------------|---------|------|
| `get_layout_signature<T>()` | ✅ 存在 | 无，但语义改为 Layout 层 |
| `get_definition_signature<T>()` | ✅ 存在 | 无 |
| `layout_signatures_match<T1,T2>()` | ✅ 存在 | 无 |
| `definition_signatures_match<T1,T2>()` | ✅ 存在 | 无 |
| `get_layout_hash<T>()` | ❌ **不存在** | hash.hpp 已在精简时删除 |
| `get_layout_verification<T>()` | ❌ **不存在** | verification.hpp 已删除 |
| `signatures_match<T1,T2>()` | ❌ **不存在** | 已重命名为 `layout_signatures_match` |
| `is_serializable_v<T,P>` | ❌ **不存在** | util/ 层已删除 |
| `has_bitfields<T>()` | ❌ **不存在** | 已删除 |
| `serialization_status<T,P>()` | ❌ **不存在** | 已删除 |
| Concepts (LayoutCompatible 等) | ❌ **不存在** | concepts.hpp 已删除 |
| `TYPELAYOUT_BIND` 宏 | ❌ **不存在** | 已删除 |
| core/ 下的 hash.hpp, verification.hpp, concepts.hpp | ❌ **不存在** | 文件已删除 |
| util/ 层所有文件 | ❌ **不存在** | 整层已删除 |

**严重差距**: project.md 描述的功能约 70% 已不存在。project.md 严重过时。

### 3.2 signature spec 声称 vs 实际实现

| Spec 需求 | 实际状态 |
|-----------|---------|
| Layout Hash Generation (`get_layout_hash`) | ❌ 不存在 |
| Layout Verification (dual hash) | ❌ 不存在 |
| Signature Comparison (`signatures_match`) | ⚠️ 重命名为 `layout_signatures_match` |
| Layout Concepts (LayoutCompatible 等) | ❌ 不存在 |
| Type Categories: `struct` prefix | ⚠️ 改为 `record` |
| Type Categories: `class` with `inherited` | ⚠️ 改为 `~base<>` 在 Definition |
| SignatureMode: Physical/Structural/Annotated 三模式 | ⚠️ 改为 Layout/Definition 两模式 |
| Physical Signature (get_physical_signature) | ⚠️ 改为 `get_layout_signature` |
| Physical Hash/Verification/Concepts/Macros | ❌ 不存在 |
| Chunked processing / Pre-sized buffer | ❌ 不存在 |
| Large struct (200 members) | ❓ 未验证 |

**严重差距**: signature spec 约 60% 的需求描述的是已删除或已重命名的功能。

### 3.3 README 准确性

README 基本准确反映了当前的双层系统和 4 个 API。但以下方面需要更新：
- 缺少 vptr 标记的说明
- 缺少组合展平的说明
- 缺少限定名的说明

## 4. 边界情况验证

| 边界情况 | Layout | Definition | 备注 |
|---------|--------|------------|------|
| 空 struct `{}` | ✅ `record[s:1,a:1]{}` | ✅ `record[s:1,a:1]{}` | 正确反映 1 字节占位 |
| 空基类 `Empty{}` + 字段 | ✅ 测试通过 | ✅ | EBO 正确处理 |
| 位域 | ✅ `@B.b:bits<W,type>` | ✅ 含名称 | 格式正确 |
| 虚继承 | ⚠️ 跳过虚基类 | ✅ `~vbase<>` | Layout 层静默跳过虚基类字段，可能导致不完整 |
| 多重继承 | ✅ 测试通过 | ✅ | 正确展平/保留 |
| 钻石继承 | ⚠️ 未测试 | ⚠️ 未测试 | 需要添加测试 |
| 模板实例化 | ✅ 正常工作 | ✅ | P2996 支持 |
| 固定数组 T[N] | ✅ | ✅ | 非字节类型用 `array<>` |
| union | ✅ | ✅ | 正确处理 |
| 匿名成员 | ✅ `<anon:N>` | ✅ | 已有专门支持 |
| alignas | ✅ | ✅ | 通过 `alignof(T)` 捕获 |
| 深嵌套组合 | ✅ 修复后展平 | ✅ 保留结构 | 测试通过 |

## 5. 实用性评估

### 5.1 IPC/共享内存场景
**可用性: ⚠️ 基本可用，有限制**
- ✅ Layout 签名可以验证两个类型是否内存兼容
- ✅ 编译时计算，零运行时开销
- ❌ 缺少 hash 功能，无法在运行时快速比较（如共享内存头部存储 hash）
- ❌ 缺少跨编译单元的签名持久化机制

### 5.2 跨编译单元一致性
**可用性: ✅ 有保证**
- 签名是 consteval 函数的返回值，对相同类型在任何编译单元中都会产生相同结果
- 前提是使用相同编译器和相同编译选项

### 5.3 增量编译友好性
**可用性: ✅ 良好**
- Header-only，类型变化时依赖该头文件的编译单元会重编译
- 签名比较是 static_assert，不通过则编译失败

### 5.4 错误信息可读性
**可用性: ✅ 良好**
- 签名是人类可读的字符串
- static_assert 失败时可以看到预期和实际签名

## 6. 发现的问题及严重级别

### 🔴 Critical

| # | 问题 | 说明 |
|---|------|------|
| C1 | **project.md 严重过时** | 约 70% 功能声明已不存在（hash, verification, concepts, util 层, 宏等）。误导开发者和用户 |
| C2 | **signature spec 严重过时** | 约 60% 需求描述的是已删除功能或旧的三模式系统 |

### 🟠 Major

| # | 问题 | 说明 |
|---|------|------|
| M1 | **虚继承 Layout 不完整** | Layout 层静默跳过虚基类字段，导致签名不包含虚基类的数据成员。两个虚继承类型可能被错误判定为匹配 |
| M2 | **缺少 hash API** | 精简后删除了 hash/verification，但这些对运行时场景（共享内存）非常重要 |

### 🟡 Minor

| # | 问题 | 说明 |
|---|------|------|
| m1 | **钻石继承未测试** | 缺少虚继承钻石问题的测试用例 |
| m2 | **大 struct (200 成员) 未验证** | spec 声称支持 200 成员，但精简后无此测试 |
| m3 | **struct 自身不编码限定名** | `ns1::Foo{int x}` 与 `ns2::Foo{int x}` 的 Definition 签名相同。这是设计选择非 bug |

## 7. 总体评估

### 核心价值是否实现？

| 维度 | 评分 | 说明 |
|------|------|------|
| Layout 签名正确性 | ⭐⭐⭐⭐ | 修复后基本完备，虚继承是唯一已知缺陷 |
| Definition 签名正确性 | ⭐⭐⭐⭐⭐ | 限定名修复后完全正确 |
| 投射关系 | ⭐⭐⭐⭐ | 语义上成立，缺少形式化验证 |
| API 最小完备性 | ⭐⭐⭐ | 4 个核心函数足够基本使用，缺 hash |
| 文档与实现一致性 | ⭐ | project.md 和 spec 严重过时 |
| 测试覆盖 | ⭐⭐⭐ | 覆盖主要场景，缺少边界测试 |

**结论**: TypeLayout 的**核心签名引擎已实现核心价值**（相同签名 ↔ 相同布局），双层系统设计合理。但**文档/spec 与实现严重脱节**，这是当前最大的技术债。

## 8. 后续建议

**优先级 P0**（阻塞）:
1. 重写 project.md，仅描述实际存在的功能
2. 重写 signature spec，仅包含实际实现的需求

**优先级 P1**（重要）:
3. 修复虚继承 Layout 签名（虚基类字段应出现在签名中）
4. 恢复 hash API（至少 `get_layout_hash<T>()` 用于运行时场景）

**优先级 P2**（改进）:
5. 添加钻石继承测试
6. 添加大 struct 测试
7. README 补充最新的签名特性说明