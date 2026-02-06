# Change: 修复代码质量问题 - 基于深度代码审查

## Why

基于一份详细的代码审查报告，发现了多个关键缺陷和改进点：

**P0 级（阻塞性）**：
1. 平台类型特化的预处理器守卫逻辑错误 - 在 Windows/MSVC 上无法编译
2. `CompileString::operator==` 潜在越界访问 - 未定义行为

**P1 级（严重）**：
3. 端序检测的默认值处理不安全 - 大端平台签名错误
4. `char[]` 特殊处理破坏布局等价性 - Structural 模式语义不一致

**P2 级（改进）**：
5. `from_number` 模板膨胀 - 编译性能
6. 循环依赖脆弱性
7. 冗余的废弃函数

**P3 级（代码风格）**：
8. 函数模板上不必要的 `static`
9. `void*` 冗余特化
10. 缺少 C++ 版本检查

## What Changes

### 关键修复 (P0)

#### 1. 用 C++20 `requires` 子句替代预处理器宏

**问题**：当前使用 `#if defined(__APPLE__)` 等宏判断是否需要 `signed char`、`long`、`long long` 的特化，但这是错误的——类型别名关系是类型系统属性，不是操作系统属性。

**现状代码**：
```cpp
#if !defined(__APPLE__) && !defined(__linux__)
template <SignatureMode Mode> struct TypeSignature<signed char, Mode> { ... };
// ❌ Windows 上 int8_t = signed char，导致重复定义
#endif
```

**修复**：
```cpp
template <SignatureMode Mode>
    requires (!std::is_same_v<signed char, int8_t>)
struct TypeSignature<signed char, Mode> { ... };
```

#### 2. 修复 `CompileString::operator==` 越界

**问题**：当 `i == N` 或 `i == M` 时访问 `value[i]` 越界。

**修复**：重写比较逻辑，确保边界安全。

### 重要改进 (P1)

#### 3. 端序检测使用 C++20 `std::endian`

```cpp
#include <bit>
#define TYPELAYOUT_LITTLE_ENDIAN (std::endian::native == std::endian::little)
```

#### 4. `char[]` 与 `std::byte[]` 签名一致性

在 Structural 模式中，所有单字节数组应统一处理。

### 其他改进 (P2/P3)

- 将 `from_number` 缓冲区从 32 减小到 22
- 移除函数模板上的 `static` 关键字
- 添加 C++20 版本检查
- 删除冗余的 `void*` 特化

## Impact

- Affected specs: `signature`
- Affected files:
  - `include/boost/typelayout/core/type_signature.hpp` - P0 修复
  - `include/boost/typelayout/core/compile_string.hpp` - P0 修复
  - `include/boost/typelayout/core/config.hpp` - P1 修复
  - `include/boost/typelayout/core/reflection_helpers.hpp` - P3 改进
