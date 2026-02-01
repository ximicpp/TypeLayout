# Cross-Process Compatibility Audit Design Document

## 1. Audit Findings Summary

### 1.1 Architecture Prefix (`get_arch_prefix()`)

**Current Implementation:**
```
[BITS-ENDIAN] format, e.g. [64-le], [32-be]
```

| 检查项 | 状态 | 发现 |
|--------|------|------|
| 指针大小识别 | ✅ 正确 | `sizeof(void*)` 正确区分 32/64-bit |
| 字节序检测 | ✅ 正确 | `__BYTE_ORDER__` (GCC/Clang) 和 fallback 都覆盖 |
| 数据模型标识 | ⚠️ **缺失** | 未区分 LP64 vs LLP64 |

**问题 #1: 数据模型未包含在 arch prefix 中**

| 数据模型 | 平台 | long 大小 | int64_t 实现 |
|----------|------|-----------|--------------|
| ILP32 | 32-bit | 4 bytes | long long |
| LP64 | Linux/macOS 64-bit | 8 bytes | long |
| LLP64 | Windows 64-bit | 4 bytes | long long |

**影响**: 如果用户类型包含 `long`，Linux 和 Windows 的签名会不同（因为 `long` 类型签名包含大小信息），但 arch prefix `[64-le]` 完全相同，无法早期识别不兼容。

**建议**: 
- 当前实现**依赖 `long` 类型签名中包含的大小信息**来区分 LP64/LLP64
- 这是**正确的设计**，因为问题的根源是 `long` 类型本身，而非架构
- **无需修改**，但应在文档中明确说明

---

### 1.2 Type Signatures (`TypeSignature<T>`)

| 检查项 | 状态 | 发现 |
|--------|------|------|
| `long` 类型 | ✅ 正确 | 包含 `[s:4,a:4]` 或 `[s:8,a:8]`，会自动区分平台 |
| `wchar_t` 类型 | ✅ 正确 | 动态计算 `sizeof`/`alignof` |
| `long double` 类型 | ✅ 正确 | 动态计算 `sizeof`/`alignof` |
| 枚举类型 | ✅ 正确 | 包含底层类型签名 |
| 数组类型 | ✅ 正确 | 包含元素类型和大小 |
| 结构体字段 | ✅ 正确 | 包含偏移、名称、类型 |
| 位域 | ✅ 正确 | `is_bit_field()` 检测，标记为不可移植 |

**结论**: 类型签名实现完整且正确。

---

### 1.3 Portable Concept

**Current Implementation:**
```cpp
template<typename T>
concept Portable = is_portable<T>();
```

| 检查项 | 状态 | 发现 |
|--------|------|------|
| 排除指针 | ❌ **问题** | 指针类型未被排除！`is_portable<int*>() = true` |
| 排除引用 | ❌ **问题** | 引用类型未被排除！ |
| 排除 `long` | ✅ 正确 | 仅在 Windows 标记 |
| 排除 `wchar_t` | ✅ 正确 | 标记为 platform_dependent |
| 排除 `long double` | ✅ 正确 | 标记为 platform_dependent |
| 排除位域 | ✅ 正确 | `has_bitfields<T>()` 检测 |
| 递归检查嵌套类型 | ✅ 正确 | 递归检查成员和基类 |

**问题 #2: 指针和引用类型被错误地视为可移植**

根据 `portability.hpp` 第 224-226 行：
```cpp
// All other types (primitives, pointers, etc.) are portable
else {
    return true;
}
```

**影响**: 包含指针字段的结构体会被 `Portable<T>` 接受，但指针在不同进程中无效。

**建议**: 修改 `is_portable()` 明确排除指针和引用类型。

---

### 1.4 Linux 上 `long` 的问题

**问题 #3: Linux 上 `long` 未被标记为 platform_dependent**

根据 `config.hpp` 第 96-101 行：
```cpp
#if defined(_WIN32) || defined(_WIN64)
    template <> struct is_platform_dependent<long> : std::true_type {};
    template <> struct is_platform_dependent<unsigned long> : std::true_type {};
#endif
// Linux LP64: long = int64_t, cannot distinguish, so don't flag
```

**分析**: 
- 在 Linux LP64 上，`long` = 8 bytes，与 `int64_t` 相同
- 但如果 Linux 代码使用 `long`，与 Windows 通信时会出问题
- 当前设计是：如果你在 Linux 上使用 `long`，编译时不会警告，但运行时 hash 会不匹配

**影响**: Linux 用户可能不知道 `long` 是危险的。

**建议**: 
- 选项 A: 总是将 `long` 标记为 platform_dependent（破坏性变更，更安全）
- 选项 B: 保持现状，依赖运行时 hash 检测（当前行为）
- 选项 C: 添加编译期警告但不阻止编译

---

### 1.5 Hash 函数

| 检查项 | 状态 | 发现 |
|--------|------|------|
| 算法 | ✅ FNV-1a | 业界标准，适合字符串 hash |
| 输出大小 | ✅ 64-bit | 冲突概率 ~1/2^64 |
| 输入完整性 | ✅ 正确 | 整个签名字符串作为输入 |

**结论**: Hash 实现正确。

---

## 2. Issue Summary

| ID | 优先级 | 问题 | 影响 | 建议 |
|----|--------|------|------|------|
| #1 | Low | Arch prefix 不含数据模型 | 文档问题 | 在文档中说明 |
| #2 | **Critical** | 指针/引用被视为可移植 | 安全漏洞 | 必须修复 |
| #3 | Medium | Linux `long` 未标记危险 | 用户体验 | 可选修复 |

---

## 3. Recommended Fixes

### Fix #2: Exclude pointers and references from Portable

**修改 `portability.hpp` 中的 `is_portable<T>()`:**

```cpp
// 在 is_platform_dependent 检查之前添加：
// Pointers are NEVER portable (different address spaces)
if constexpr (std::is_pointer_v<CleanT>) {
    return false;
}
// References are NEVER portable
if constexpr (std::is_reference_v<T>) {  // Note: T not CleanT
    return false;
}
```

### Fix #3: Always flag `long` as platform-dependent (Optional)

**修改 `config.hpp`:**

```cpp
// Remove the #if defined(_WIN32)... condition
// Always mark long as platform-dependent
template <> struct is_platform_dependent<long> : std::true_type {};
template <> struct is_platform_dependent<unsigned long> : std::true_type {};
```

---

## 4. Documentation Gaps

| 文档位置 | 缺失内容 |
|----------|----------|
| README | 应明确说明 "跨平台通信时必须使用固定宽度类型" |
| README | 应列出所有不可移植类型清单 |
| API 注释 | `Portable<T>` 应说明排除了哪些类型 |
