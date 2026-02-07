# FixedString<N> 实现分析报告

## 1. 逐函数正确性分析

### 1.1 默认构造函数 — ✅ 正确
```cpp
constexpr FixedString() : value{} {}
```
值初始化，所有字节清零。无问题。

### 1.2 字符数组构造函数 — ✅ 正确
```cpp
constexpr FixedString(const char (&str)[N]) {
    for (size_t i = 0; i < N; ++i) value[i] = str[i];
}
```
模板参数 N 与数组大小精确匹配（CTAD），包括 '\0'。无越界风险。

### 1.3 string_view 构造函数 — ⚠️ 有设计问题
```cpp
constexpr FixedString(std::string_view sv) : value{} {
    for (size_t i = 0; i < N - 1 && i < sv.size(); ++i) value[i] = sv[i];
}
```
**问题**：当 `sv.size() < N - 1` 时，尾部已被 `value{}` 清零，正确。但 N 必须由调用方显式指定（如 `FixedString<10>(sv)`），CTAD 无法从 `string_view` 推导 N。
**实际使用**：在 `qualified_name_for()` 中使用 `FixedString<self.size() + 1>(self)`——正确。
**结论**：功能正确，但调用方必须手动计算 N，这是必要的（编译时确定大小）。

### 1.4 from_number() — ⚠️ 有效率问题
```cpp
static constexpr FixedString<32> from_number(T num) noexcept { ... }
```
**正确性**：✅ 逻辑正确，处理了 0、正数、负数、反转。
**问题 1 — 返回类型固定 32 字节**：
- `from_number` 始终返回 `FixedString<32>`，即使数字只有 1-2 位
- 调用方写 `FixedString<number_buffer_size>::from_number(x)`（N=22），但函数内部硬编码返回 `FixedString<32>`
- 模板参数 N=22 在 `from_number` 中**完全没用**，只是借壳访问静态方法
- 每次 `operator+` 时，32 字节都会参与拼接计算，浪费 constexpr 步数

**问题 2 — 反转循环增加步数**：
- 先正序写入再反转，需要 `2 × digits` 次操作
- 可以改为从右往左直接写入，省掉反转

**影响量化**：
- 一个典型字段签名调用 `from_number` 2-3 次（offset, size, align）
- 100 个字段 × 3 次 = 300 次 `from_number`
- 每次产生 `FixedString<32>`，但实际内容通常只有 1-3 字符
- 在 `operator+` 中多余的 ~29 字节零拷贝 × 300 = ~8700 多余步数

### 1.5 operator+ — ⚠️ 核心瓶颈
```cpp
template <size_t M>
constexpr auto operator+(const FixedString<M>& other) const noexcept {
    constexpr size_t new_size = N + M - 1;
    char result[new_size] = {};
    // ... 拷贝 this, 拷贝 other ...
    return FixedString<new_size>(result);
}
```
**正确性**：✅ 逻辑正确。
**效率问题**：
1. **模板参数膨胀**：每次拼接 `new_size = N + M - 1`，但实际有效字符可能远少于 N。例如 `from_number(4)` 返回 `FixedString<32>` 但只有 1 个有效字符。拼接后 `new_size` 包含大量死空间。
2. **左操作数扫描**：`while (pos < N - 1 && value[pos] != '\0')` 需要逐字符找到有效内容末尾。如果 `N` 很大（因为 `from_number` 的 32 字节膨胀），即使内容只有 "4"，也需要额外的零检查。
3. **O(n²) 累积**：fold expression `(a + b + c + d + ...)` 每次产生更大的中间结果，拷贝量逐次增长。这是 constexpr 步数限制的根因。

**具体膨胀示例**：
```
FixedString{"@"}          → N=2
+ from_number(0)          → M=32, new_size=33  (实际 "@0" 只有 2 字符)
+ FixedString{":"}        → M=2,  new_size=34  (实际 "@0:" 只有 3 字符)
+ from_number(4)          → M=32, new_size=65  (实际 "@0:i32[s:4" 可能 10 字符)
```
→ 模板参数 65，实际内容 10 字符。**6.5× 膨胀率**。

### 1.6 operator== — ✅ 正确
```cpp
template <size_t M>
constexpr bool operator==(const FixedString<M>& other) const noexcept { ... }
```
逐字符比较，遇到 '\0' 即停止。允许不同 N/M 之间比较。正确且高效。

### 1.7 operator==(const char*) — ⚠️ 潜在越界
```cpp
constexpr bool operator==(const char* other) const noexcept {
    for (size_t i = 0; i < N; ++i) {
        if (value[i] != other[i]) return false;
        if (value[i] == '\0') return true;
    }
    return other[N] == '\0';  // ← 如果 other 长度 < N 且无 '\0' 匹配？
}
```
**问题**：如果 `other` 的有效内容恰好 == N-1 个字符（无提前 '\0' 退出），则最后访问 `other[N]`。这要求 `other` 至少有 N+1 字节。对于字面量 `"abc"` 比较 `FixedString<4>` 是安全的（字面量自带 '\0'），但语义上有微妙的正确性依赖。
**实际风险**：低，因为所有调用方都使用字符串字面量。但代码不够防御性。

### 1.8 length() — ✅ 正确但可优化
线性扫描找 '\0'，O(N)。在 constexpr 环境中无法避免，但如果缓存长度可以避免重复扫描。

### 1.9 skip_first() — ✅ 正确
左移一个字符，用于去除 fold expression 产生的前导逗号。N 不变（不缩小），多一个尾部 '\0'。正确。

## 2. 效率问题总结

| 问题 | 严重程度 | 影响 |
|------|---------|------|
| `from_number` 返回固定 `FixedString<32>` | 🔴 高 | 每次拼接多余 ~29 字节参与计算，模板膨胀 |
| `operator+` 模板参数累积膨胀 | 🔴 高 | 100 字段类型的签名 N 可达数千，拷贝量 O(n²) |
| `from_number` 反转循环 | 🟡 中 | 每次多 ~digits 步，300 次 ≈ 额外 ~1000 步 |
| `operator==(const char*)` 边界 | 🟡 中 | 潜在越界，实际无触发场景 |

## 3. 优化建议

### 优化 A：`from_number` 返回紧凑大小（高收益）

**当前**：始终返回 `FixedString<32>`
**优化**：返回 `FixedString<21>`（uint64_t 最大 20 位 + '\0'）

```cpp
static constexpr FixedString<21> from_number(T num) noexcept { ... }
```

**收益**：每次 `from_number` 拼接减少 11 字节死空间，300 次调用 ≈ 减少 3300 字节模板参数膨胀。

> 更激进的方案：编译时计算实际位数，返回 `FixedString<digits+1>`。但这需要两阶段 consteval（先算位数，再构造），Bloomberg Clang 可能不支持。保守起见，固定为 21。

### 优化 B：`from_number` 消除反转（中收益）

**当前**：正序写入 → 反转
**优化**：从右往左写入，无需反转

```cpp
static constexpr FixedString<21> from_number(T num) noexcept {
    char result[21] = {};
    int end = 20;  // 从右边开始
    // ... 从右往左填充 ...
    // 然后左移到起始位置
}
```

**收益**：省掉 `idx/2` 次交换操作。但需要最后一次左移拷贝。净收益约 30-50%。

### 优化 C：`operator+` 左操作数扫描优化（中收益）

**当前**：`while (pos < N - 1 && value[pos] != '\0')` 逐字符扫描
**优化**：如果存储实际长度（在 `value[N-1]` 或额外成员），可以直接跳到拷贝终点。

**代价**：增加结构体大小或改变内存布局。对 header-only 库不太合适。
**结论**：不推荐，收益不够大。

### 优化 D：修复 `operator==(const char*)` 边界（低成本高价值）

```cpp
constexpr bool operator==(const char* other) const noexcept {
    for (size_t i = 0; i < N; ++i) {
        if (value[i] != other[i]) return false;
        if (value[i] == '\0') return true;
    }
    return true;  // value 完全匹配且无 '\0'（N 字节全满），视为相等
}
```

或者更安全的做法：循环结束后检查 `other` 对应位置是否也是 '\0'：

```cpp
// 保持现有逻辑，但此分支实际不可达（FixedString 总是包含 '\0'）
```

实际上 FixedString 保证 `value[N-1] == '\0'`（构造函数保证），所以循环一定会在 `value[i] == '\0'` 时退出。最后的 `return other[N] == '\0'` **不可达**。可以简化为 unreachable 或删除。

## 4. 推荐实施优先级

| 优先级 | 优化项 | 预期收益 | 风险 |
|--------|--------|---------|------|
| P0 | 优化 A：`from_number` 返回 `FixedString<21>` | 减少 ~35% 模板膨胀 | 极低 |
| P1 | 优化 D：修复 `operator==(const char*)` | 消除潜在 UB | 极低 |
| P2 | 优化 B：`from_number` 消除反转 | 减少 ~10% constexpr 步数 | 低 |
| P3 | 优化 C：存储长度 | 减少扫描开销 | 中（改变结构） |
