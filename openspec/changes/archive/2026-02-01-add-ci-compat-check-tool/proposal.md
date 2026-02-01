# Change: Add CI Cross-Platform Compatibility Check Tool

## Why

TypeLayout 的核心价值是检测跨进程/跨机器的类型布局兼容性。目前用户需要手动编写代码调用 `get_layout_hash<T>()` 并比对，没有开箱即用的工具。

用户需要一个**零配置**的方案：
- 只定义类型和目标平台
- CI 自动在多平台编译并比对签名
- 不兼容时自动失败并报告

## What Changes

### 新增组件

1. **`include/boost/typelayout/compat.hpp`** - 用户配置接口
   - `TYPELAYOUT_TYPES(...)` - 注册需要检查的类型
   - `TYPELAYOUT_PLATFORMS(...)` - 定义目标平台集合

2. **`tools/siggen.cpp`** - 签名生成器模板
   - 编译时 include 用户配置
   - 输出当前平台所有类型的签名

3. **`tools/compare_signatures.cpp`** - 签名比对工具
   - 纯 C++17，无需反射
   - 读取多个签名文件并比对
   - 输出兼容性报告

4. **`.github/workflows/compat-check.yml`** - 可复用 Workflow
   - 支持 `workflow_call` 被其他仓库调用
   - 自动在多平台生成签名并比对

## Impact

- **新增 Spec**: `specs/compat-tool/spec.md`
- **修改 Spec**: `specs/ci/spec.md` (添加 reusable workflow)
- **新增文件**:
  - `include/boost/typelayout/compat.hpp`
  - `tools/siggen.cpp`
  - `tools/compare_signatures.cpp`
  - `.github/workflows/compat-check.yml`
- **文档更新**: README 添加使用说明

## User Experience

### 用户最少只需 2 步：

**Step 1: 创建配置文件**
```cpp
// typelayout.config.hpp
#include <boost/typelayout/compat.hpp>

struct MyData { int32_t id; float value; };

TYPELAYOUT_TYPES(MyData)
TYPELAYOUT_PLATFORMS(linux_x64, windows_x64)  // 可选
```

**Step 2: 添加 3 行 workflow**
```yaml
jobs:
  check:
    uses: aspect-labs/typelayout/.github/workflows/compat-check.yml@v1
```

### 高级用法
```yaml
jobs:
  check:
    uses: aspect-labs/typelayout/.github/workflows/compat-check.yml@v1
    with:
      config: 'src/protocol/types.hpp'
      platforms: 'linux-x64,linux-arm64,windows-x64'
```
