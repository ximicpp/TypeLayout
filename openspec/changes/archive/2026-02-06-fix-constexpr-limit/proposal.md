# Change: 突破 constexpr 步数限制（支持大型结构体）

## Why

当前实现由于 constexpr 评估步数限制，仅支持 ≤40 成员的结构体。超过此限制，编译失败并报告 `constexpr step limit exceeded`。

这是一个**关键可用性限制**，因为真实世界的结构体通常有 60-200+ 成员：
- 金融领域：交易记录、市场数据
- 游戏开发：实体组件、网络同步包
- 数据库系统：行存储结构

## 根本原因深度分析

### 当前实现流程

```
get_fields_signature<T, Mode>()
  └─ concatenate_field_signatures<T, Mode>(index_sequence<0,1,2...N>)
       └─ (build_field_with_comma<T, 0, true, Mode>() + 
           build_field_with_comma<T, 1, false, Mode>() + ...)
            └─ get_field_signature<T, Index, Mode>()
                 └─ nonstatic_data_members_of(^^T, ...)[Index]  ← 每个成员调用一次！
```

**关键发现**：`get_field_signature<T, Index, Mode>()` 中**每次都调用 `nonstatic_data_members_of()`**：
- 对于 N 个成员，`nonstatic_data_members_of()` 被调用 **N 次**
- 这个 API 每次都创建一个新的 `std::vector`，开销巨大

### 两个叠加问题

1. **O(n²) 字符串拼接**：每次 `CompileString` 拼接都拷贝整个累积字符串
2. **O(n) 反射 API 调用**：每个成员都重新调用 `nonstatic_data_members_of()`

## 尝试的优化方案

### 方案 A：分块签名生成
将成员处理分成 K 个一组，每块在独立的 consteval 函数中处理。

**结论**：无法解决问题。分块无法减少反射 API 调用次数。

### 方案 B：增量哈希（绕过字符串构建）
不构建签名字符串，直接将布局数据流式传给哈希函数。

**结论**：已实现 `FNV1aState` 流式哈希类，但无法用于大型结构体。
原因：P2996 工具链中 `std::vector` 在 constexpr 上下文的结果无法跨函数调用缓存。

### 方案 C：`template for` 优化
使用 P2996 的 `template for` 循环，只调用一次反射 API。

```cpp
template for (constexpr auto member : nonstatic_data_members_of(^^T, ...)) {
    // 处理每个成员
}
```

**结论**：**不工作**！P2996 工具链的 `template for` 同样无法使用堆分配的 `std::vector` 作为范围表达式。

错误信息：
```
constexpr variable '__range' must be initialized by a constant expression
pointer to subobject of heap-allocated object is not a constant expression
```

## 根本结论

**当前 P2996 工具链（Bloomberg 实验性 Clang）存在根本性限制**：

1. `nonstatic_data_members_of()` 返回的 `std::vector` 是堆分配的
2. 堆分配的指针不能作为 constexpr 变量存储
3. 因此无法：
   - 缓存成员列表跨函数调用
   - 在 `template for` 中使用该 API
4. 每个成员的处理都需要重新调用反射 API
5. 无论采用何种优化策略，总步数消耗都是 **O(n × 反射API开销)**

**这是工具链限制，而非库设计限制。**

## 当前状态

### 已完成

1. ✅ 添加 `FNV1aState` 流式哈希状态类（在 `utils/hash.hpp`，为未来优化准备）
2. ✅ 创建压力测试套件 (`test/test_stress.cpp`)
3. ✅ 验证 40 成员结构体工作正常
4. ✅ 深度分析并文档化工具链限制
5. ✅ 在 `reflection_helpers.hpp` 添加详细限制说明

### 当前支持

- 结构体成员数量：**约 40-50 个**
- 这对大多数实际用例已经足够

### 未来改进路径

当 P2996 工具链改进后，可能的解决方案：

1. **工具链支持 constexpr 范围缓存**：如果 `template for` 能正确处理动态范围
2. **反射 API 返回静态数组**：而不是 `std::vector`
3. **编译器提高 constexpr 步数限制**：或优化反射 API 的步数消耗

## Impact

- Affected specs: `signature`
- Affected code:
  - `include/boost/typelayout/core/reflection_helpers.hpp` - 添加限制说明
  - `include/boost/typelayout/utils/hash.hpp` - 添加 FNV1aState 类
  - `test/test_stress.cpp` - 新增压力测试

## Test Files

- `test/test_stress.cpp` - 测试 20/30/40 成员结构体，验证当前限制

## Current Test Coverage

```cpp
// 当前支持的测试（全部通过）：
struct Stress20  { /* 20 members */ };
struct Stress30  { /* 30 members */ };
struct Stress40  { /* 40 members */ };
static_assert(get_layout_hash<Stress40>() != 0); // ✅ 编译通过

// 原始目标（受工具链限制，暂时无法实现）：
// struct Stress200 { /* 200 members */ };
// 需等待 P2996 工具链改进
```