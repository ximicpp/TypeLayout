# Change: Refactor Cross-Platform Compatibility Toolchain

## Why

当前跨平台兼容检测工具链存在以下核心问题：
1. Bash 脚本承担过重职责（正则扫描、源码生成、编译调用）
2. TypeEntry 重复定义 + `reinterpret_cast` UB
3. Phase 2 需要 Bash 动态生成 C++ 源码
4. `static_assert` 编译时路径未被工具链利用

不需要向后兼容。使用宏作为唯一推荐路径。

## What Changes

### 1. 新增 `sig_types.hpp` — 共享类型定义

```cpp
// include/boost/typelayout/tools/sig_types.hpp
namespace boost::typelayout {
    struct TypeEntry { ... };
    struct PlatformInfo { ... };
}
```

消除 .sig.hpp 和 compat_check.hpp 之间的 TypeEntry 重复定义。

### 2. 新增 `TYPELAYOUT_EXPORT_TYPES(...)` 宏

用户写一个 3 行 .cpp 完成 Phase 1 类型注册：

```cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "my_types.hpp"
TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord, ns::MyType)
```

取代 Bash 正则扫描 + 模板文件。删除 `sig_export_template.cpp.in`。

### 3. 新增 `compat_auto.hpp` + `TYPELAYOUT_CHECK_COMPAT(...)` 宏

用户写几行 .cpp 完成 Phase 2 兼容性检查：

```cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>
TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang)
```

取代 Bash 动态生成 C++ 源码。删除 `compat_check_template.cpp.in`。

同时提供 `TYPELAYOUT_ASSERT_COMPAT(...)` 用于纯编译时 static_assert 路径。

### 4. 更新 .sig.hpp 格式

- 引用 `sig_types.hpp` 而非内嵌 TypeEntry
- 添加 `get_platform_info()` 便捷函数
- 更新 example/sigs/ 下的预生成文件

### 5. 精简 Bash CLI

- 删除源码生成能力（不再有 `generate_export_source()`）
- 删除模板文件 (`sig_export_template.cpp.in`, `compat_check_template.cpp.in`)
- `export` 子命令：编译运行用户的 export .cpp
- `compare` 子命令：编译运行用户的 check .cpp
- `check` 子命令：在各平台编译用户的 export .cpp + 编译运行 check .cpp

### 6. 更新 CMake、CI 和文档

- CMake: 新增 `typelayout_add_compat_pipeline()`
- CI: 工作流接受 `export_source` 和 `check_source` 输入
- 文档和示例全面更新

## Impact

- **删除文件**:
  - `tools/sig_export_template.cpp.in`
  - `tools/compat_check_template.cpp.in`
- **新增文件**:
  - `include/boost/typelayout/tools/sig_types.hpp`
  - `include/boost/typelayout/tools/compat_auto.hpp`
- **大幅修改**:
  - `include/boost/typelayout/tools/sig_export.hpp` (添加宏，更新生成器)
  - `include/boost/typelayout/tools/compat_check.hpp` (使用共享 TypeEntry)
  - `tools/typelayout-compat` (精简)
  - `cmake/TypeLayoutCompat.cmake` (增强)
  - `.github/workflows/compat-check.yml` (更新输入参数)
  - `example/sigs/*.sig.hpp` (重新生成)
  - `example/cross_platform_check.cpp` (使用宏)
  - `example/compat_check.cpp` (使用宏)
- **不影响**: 核心签名引擎 (`core/`)