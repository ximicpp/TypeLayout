# Change: Add Cross-Platform Compatibility Toolchain

## Why

当前 Phase 1+2 管线需要用户手动：在每个平台编译 sig_export、运行它、收集 .sig.hpp、
编写 compat_check.cpp 包含所有签名头、编译检查。这个流程步骤多且容易出错。

用户需要一个**一键式工具链**：选择平台集合 → 工具链自动构建环境、导出签名、比对结果。

## What Changes

### 1. Bash CLI 工具: `tools/typelayout-compat`

```bash
# 本地一键检查：用户提供类型定义头文件 + 目标平台列表
typelayout-compat check \
  --types my_types.hpp \
  --platforms x86_64-linux-clang,arm64-macos-clang \
  --output build/sigs

# 仅导出当前平台签名
typelayout-compat export --types my_types.hpp --output sigs/

# 仅比对已有签名
typelayout-compat compare --sigs build/sigs/ --report
```

核心流程：
1. 读取平台注册表 (`tools/platforms.conf`)，获取各平台的 Docker 镜像 / 构建命令
2. 对每个目标平台：在 Docker 容器中编译 sig_export、运行、收集 .sig.hpp
3. 自动生成 Phase 2 的 `_compat_check.cpp`，包含所有签名头
4. 编译运行，输出兼容性报告

### 2. 平台注册表: `tools/platforms.conf`

```ini
[x86_64-linux-clang]
docker_image = ghcr.io/ximicpp/typelayout-p2996:latest
env = LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu
compiler = clang++
flags = -std=c++26 -freflection -freflection-latest -stdlib=libc++

[arm64-macos-clang]
mode = local
compiler = clang++
flags = -std=c++26 -freflection -freflection-latest
```

### 3. GitHub Actions 可复用工作流更新: `compat-check.yml`

```yaml
# 外部仓库调用
jobs:
  compat:
    uses: ximicpp/TypeLayout/.github/workflows/compat-check.yml@main
    with:
      types_header: include/protocol/types.hpp
      platforms: x86_64-linux-clang,arm64-macos-clang
```

更新为使用新的 `sig_export.hpp` + `compat_check.hpp` 工具链，
生成 `.sig.hpp` 而非 `.txt`，Phase 2 使用 `static_assert` 编译时检查。

### 4. 用户类型注册模板: `tools/sig_export_template.cpp.in`

CMake/脚本 configure 时替换 `@TYPES_HEADER@` 和 `@TYPE_REGISTRATIONS@`：
```cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "@TYPES_HEADER@"
int main(int argc, char* argv[]) {
    boost::typelayout::SigExporter ex;
    @TYPE_REGISTRATIONS@
    // ...
}
```

## Impact

- **新增文件**:
  - `tools/typelayout-compat` — Bash CLI 主入口
  - `tools/platforms.conf` — 平台注册表
  - `tools/sig_export_template.cpp.in` — 导出程序模板
  - `tools/compat_check_template.cpp.in` — 检查程序模板
- **修改文件**:
  - `.github/workflows/compat-check.yml` — 更新为新工具链
- **受影响 spec**: `cross-platform-compat` (MODIFIED)
- **非破坏性**: 不修改核心签名引擎和现有 tools/ 头文件
