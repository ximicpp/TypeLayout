## Context

重构跨平台兼容检测工具链。不需要向后兼容，彻底清理。
使用宏作为唯一推荐路径，将逻辑从 Bash 推回 C++。

### 约束
- Header-only Boost 库设计
- Phase 1 需要 P2996 Clang，Phase 2 只需 C++17
- 不引入 Python 或其他非 C++ 依赖

## Goals / Non-Goals

### Goals
1. 消除 TypeEntry 重复定义 + `reinterpret_cast` UB
2. 提供 C++ 宏 API 用于类型注册和兼容检查
3. 精简 Bash CLI 为纯编排层
4. 让 `static_assert` 成为一等公民

### Non-Goals
- 不替换 Bash CLI（保留作为便捷编排入口）
- 不修改核心签名引擎
- 不做 GUI 工具

## Decisions

### Decision 1: `sig_types.hpp` — 共享类型定义

创建一个纯 C++17 头文件，定义 `TypeEntry` 和 `PlatformInfo`。

```cpp
// sig_types.hpp — C++17, no P2996
namespace boost { namespace typelayout {

struct TypeEntry {
    const char* name;
    const char* layout_sig;
    const char* definition_sig;
};

struct PlatformInfo {
    const char*      platform_name;
    const char*      arch_prefix;
    const TypeEntry* types;
    int              type_count;
    std::size_t      pointer_size;
    std::size_t      sizeof_long;
    std::size_t      sizeof_wchar_t;
    std::size_t      sizeof_long_double;
    std::size_t      max_align;
};

}} // namespace
```

.sig.hpp 和 compat_check.hpp 都通过 `#include` 引用同一个定义。

### Decision 2: `TYPELAYOUT_EXPORT_TYPES(...)` — Phase 1 宏

宏展开为完整的 `main()` 函数，用户只需 3 行代码：

```cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "my_types.hpp"
TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord)
```

实现使用自包含的 FOR_EACH 可变参数展开（不依赖 Boost.PP）。

### Decision 3: `TYPELAYOUT_CHECK_COMPAT(...)` — Phase 2 宏

两个变体：

```cpp
// 运行时报告模式
TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang)
// → 生成 main()，调用 CompatReporter::print_report()

// 编译时断言模式
TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, arm64_macos_clang)
// → 生成 static_assert，编译通过 = 所有类型兼容
```

需要 .sig.hpp 提供 `get_platform_info()` 便捷函数。

### Decision 4: .sig.hpp 格式升级

生成的 .sig.hpp 不再内嵌 TypeEntry 定义，改为：

```cpp
#include <boost/typelayout/tools/sig_types.hpp>

namespace boost { namespace typelayout { namespace platform {
namespace x86_64_linux_clang {

// ... 签名数据 (不变) ...

// 新增: 便捷访问器
inline constexpr PlatformInfo get_platform_info() {
    return { platform_name, arch_prefix, types, type_count,
             pointer_size, sizeof_long, sizeof_wchar_t,
             sizeof_long_double, max_align };
}

}}}} // namespace
```

### Decision 5: Bash CLI 精简

CLI 不再做任何源码生成，只做编排：

| 子命令 | 职责 |
|--------|------|
| `export --source <file.cpp>` | 在指定平台（Docker/local）编译运行用户的 export 程序 |
| `compare --source <file.cpp>` | 在本地编译运行用户的 check 程序 |
| `check --export-source ... --check-source ... --platforms ...` | 组合上述两步 |

删除所有模板文件和源码生成逻辑。

### Decision 6: FOR_EACH 宏实现策略

使用经典的递归宏技术实现 FOR_EACH，不依赖 Boost.PP：

```cpp
// 计数宏 (支持最多 64 个参数)
#define TYPELAYOUT_DETAIL_NARG(...) TYPELAYOUT_DETAIL_NARG_(__VA_ARGS__, TYPELAYOUT_DETAIL_RSEQ_N())
// ... 展开逻辑 ...

#define TYPELAYOUT_DETAIL_FOR_EACH(macro, ...) \
    TYPELAYOUT_DETAIL_CAT(TYPELAYOUT_DETAIL_FOR_EACH_, TYPELAYOUT_DETAIL_NARG(__VA_ARGS__))(macro, __VA_ARGS__)
```

在 MSVC 上需要 `/Zc:preprocessor`（C++17 标准预处理器），这是合理要求。

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| 用户需要写 3 行 .cpp 而非零配置 | 宏极简，且可从文档复制粘贴；比 protobuf/flatbuffers 的工作量少一个数量级 |
| MSVC 需要 /Zc:preprocessor | MSVC 17.0+ 默认启用，合理要求 |
| .sig.hpp 依赖 TypeLayout include path | Phase 2 本来就需要 |

## Open Questions

1. **`get_platform_info()` 是 constexpr 还是 inline**？
   - constexpr 更好（支持 static_assert 路径），但需确认所有字段在 constexpr 上下文可用

2. **`TYPELAYOUT_ASSERT_COMPAT` 如何报告哪个类型不匹配**？
   - static_assert 的 message 参数需要是字符串字面量，无法动态拼接
   - 可以为每个类型单独展开一个 static_assert，失败时显示类型名