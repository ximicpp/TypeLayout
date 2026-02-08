## Context

### 核心问题

TypeLayout 在编译时生成签名，但签名中的 `sizeof`/`alignof` 值由**当前编译器在当前平台**决定。
无法在 x86_64 Linux 的编译器上得到 ARM32 Windows 的 `sizeof(wchar_t)`。
因此，不可能在单次编译中"模拟"另一个平台的签名。

### 解决方案：两阶段编译管线

将跨平台比较拆分为两个编译阶段：

```
Phase 1: Signature Export (per platform, requires P2996 compiler)
  ┌───────────────────────┐
  │ 在 Platform X 上编译    │ ──► 运行 ──► x86_64_linux_clang.sig.hpp
  │ sig_export.cpp         │         (constexpr const char[] 签名数据)
  └───────────────────────┘
  ┌───────────────────────┐
  │ 在 Platform Y 上编译    │ ──► 运行 ──► arm64_linux_clang.sig.hpp
  │ sig_export.cpp         │
  └───────────────────────┘

Phase 2: Compatibility Check (any platform, does NOT require P2996)
  ┌───────────────────────────────────────┐
  │ #include "x86_64_linux_clang.sig.hpp" │
  │ #include "arm64_linux_clang.sig.hpp"  │
  │                                       │
  │ static_assert(layout_match(           │
  │   plat_a::Packet_layout,              │
  │   plat_b::Packet_layout));            │
  │                                       │
  │ // 编译通过 = 二进制兼容              │
  │ // 编译失败 = 签名不匹配             │
  └───────────────────────────────────────┘
```

关键洞察：**Phase 2 不需要 P2996 编译器**，因为 `.sig.hpp` 只是普通的 `constexpr const char[]`，
任何 C++17+ 编译器都能编译比较。这使得兼容性检查可以在 CI/CD 中轻松集成。

## Goals / Non-Goals

### Goals

1. **纯 C++ 方案**：从签名导出到兼容性检查，全流程不依赖 Python 或其他语言
2. **编译时检查**：兼容性判断结果通过 `static_assert` 在编译期给出
3. **实际签名比对**：使用 TypeLayout 库生成的真实签名，而非字段级启发式分析
4. **平台自动检测**：通过编译器预定义宏自动生成 `arch_os_compiler` 平台标识
5. **运行时报告**：可选的详细报告模式，输出人类可读的兼容性矩阵
6. **构建系统集成**：提供 CMake 模块简化多平台工作流

### Non-Goals

1. **不模拟其他平台**：不尝试在一个平台上推断另一个平台的类型大小
2. **不修改核心签名引擎**：工具层只消费签名，不改变签名生成逻辑
3. **不替代序列化库**：只判断是否需要序列化，不提供序列化实现
4. **不支持运行时动态比较**：签名是编译时常量，不从文件动态加载

## Decisions

### D1: 生成的签名使用 `constexpr const char[]` 而非 `FixedString<N>`

**选择**：`inline constexpr const char TYPE_layout[] = "...";`

**原因**：
- `FixedString<N>` 的模板参数 N 在不同平台可能不同（同一类型的签名长度可能因 `sizeof(long)` 不同而不同）
- `const char[]` 是最通用的 C++ 字符串表示，任何编译器都能处理
- Phase 2 不需要 P2996 编译器，降低集成门槛
- `std::string_view` 的 `constexpr operator==` 可以在编译时比较不等长字符串

**替代方案**：
- `constexpr std::string_view`：不能用于 NTTP，且在 C++17 constexpr 支持有限
- `FixedString<MAX_SIG_LEN>`：浪费空间，且需要约定最大长度

### D2: 平台命名规范 `{arch}_{os}_{compiler}`

**选择**：三段式命名 `x86_64_linux_clang`

**原因**：
- 直观可读，一眼看出目标平台
- 可用作 C++ namespace 标识符（全小写+下划线）
- 三个维度（架构、OS、编译器）完整描述了影响类型布局的因素
- 与 CMake 的 `CMAKE_SYSTEM_PROCESSOR` / `CMAKE_SYSTEM_NAME` 对应

**已知平台标识**：
| 标识 | 架构 | 操作系统 | 编译器 |
|------|------|---------|--------|
| `x86_64_linux_clang` | x86-64 | Linux | Clang |
| `x86_64_linux_gcc` | x86-64 | Linux | GCC |
| `arm64_linux_clang` | AArch64 | Linux | Clang |
| `arm64_linux_gcc` | AArch64 | Linux | GCC |
| `x86_64_windows_msvc` | x86-64 | Windows | MSVC |
| `x86_64_windows_clang` | x86-64 | Windows | Clang-cl |
| `arm64_macos_clang` | AArch64 | macOS | Apple Clang |
| `x86_64_macos_clang` | x86-64 | macOS | Apple Clang |

### D3: `.sig.hpp` 同时包含逐类型变量和注册表数组

**选择**：同时提供两种访问方式

```cpp
// 逐类型变量 — 用于 static_assert 编译时检查
inline constexpr const char PacketHeader_layout[] = "...";

// 注册表数组 — 用于运行时报告
struct TypeEntry {
    const char* name;
    const char* layout_sig;
    const char* definition_sig;
};
inline constexpr TypeEntry types[] = { ... };
inline constexpr int type_count = N;
```

**原因**：
- 编译时检查需要具名变量（`static_assert` 不能遍历数组）
- 运行时报告需要可迭代的数据结构

### D4: `constexpr sig_match` 使用 `std::string_view` 比较

**选择**：

```cpp
constexpr bool sig_match(const char* a, const char* b) {
    return std::string_view(a) == std::string_view(b);
}
```

**原因**：
- `std::string_view::operator==` 在 C++17 起就是 `constexpr`
- 自动处理不等长字符串
- 简单直接，无需自定义比较逻辑

### D5: SigExporter 使用运行时 I/O 生成头文件

**选择**：`SigExporter` 是一个运行时类，使用 `<fstream>` 写文件

**原因**：
- C++ 没有编译时文件写入能力
- Phase 1 本身就是"编译+运行"，运行时 I/O 是唯一选择
- 签名值在编译时通过 `consteval` 获取，只有"写入文件"这一步是运行时
- 使用模板函数 `add<T>()` 获取编译时签名，转为运行时字符串写出

## Risks / Trade-offs

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| Phase 1 需要在每个目标平台实际编译运行 | 需要目标平台的编译环境（Docker/CI） | 提供 Docker 多平台构建示例 + CI 模板 |
| 签名字符串中的特殊字符需要转义 | 生成的 .hpp 可能有语法错误 | SigExporter 对 `\` 和 `"` 进行转义 |
| 不同版本的 TypeLayout 可能生成不同签名 | 版本不一致导致误判 | 在 .sig.hpp 中记录 TypeLayout 版本 |
| 签名字符串可能很长（>1000 字符） | 编译器对字符串字面量有长度限制 | 大多数编译器支持 64KB+ 字符串字面量，足够 |

## Open Questions

1. ~~是否需要支持 JSON 格式的导出？~~ **决定：不需要**，`.sig.hpp` 格式既满足编译时检查又满足运行时报告
2. 是否应在 `.sig.hpp` 中嵌入 TypeLayout 库版本号用于版本校验？
