# Change: 拆分头文件为模块化结构

## Why
当前 `typelayout.hpp` 是一个 1240 行的单文件，包含所有功能实现。这对于 Boost 代码审查来说过于庞大，难以理解和维护。拆分为模块化结构可以：
- 提高代码可读性和可维护性
- 便于 Boost 社区审查
- 支持按需包含特定功能

## What Changes
1. 将单一头文件拆分为多个逻辑模块
2. 保持向后兼容性（主头文件包含所有模块）
3. 支持细粒度包含

## 详细设计

### 1. 新目录结构

```
include/boost/
├── typelayout.hpp                    # 便捷头文件 (包含所有)
└── typelayout/
    ├── typelayout.hpp                # 主头文件 (包含所有)
    ├── fwd.hpp                       # 前向声明
    ├── config.hpp                    # 平台检测和配置
    ├── compile_string.hpp            # CompileString 模板
    ├── type_signature.hpp            # TypeSignature 特化
    ├── reflection_helpers.hpp        # P2996 反射辅助函数
    ├── signature_generation.hpp      # 签名生成逻辑
    ├── hash.hpp                      # 哈希算法 (FNV-1a, DJB2)
    ├── verification.hpp              # LayoutVerification 结构
    ├── portability.hpp               # 可移植性检查
    ├── concepts.hpp                  # C++20 Concepts
    └── interprocess.hpp              # Boost.Interprocess 集成 (可选)
```

### 2. 模块职责

| 模块 | 行数估计 | 职责 |
|------|----------|------|
| `config.hpp` | ~100 | 平台检测、宏定义、static_assert |
| `compile_string.hpp` | ~150 | CompileString 模板类 |
| `type_signature.hpp` | ~300 | 所有类型的 TypeSignature 特化 |
| `reflection_helpers.hpp` | ~150 | P2996 API 封装函数 |
| `signature_generation.hpp` | ~100 | get_layout_signature 等公共 API |
| `hash.hpp` | ~80 | 哈希算法实现 |
| `verification.hpp` | ~50 | LayoutVerification 结构 |
| `portability.hpp` | ~200 | is_portable, has_bitfields |
| `concepts.hpp` | ~50 | Portable, LayoutCompatible 等 |
| `interprocess.hpp` | ~30 | offset_ptr 特化 |

### 3. 包含关系

```
typelayout.hpp (便捷)
    └── typelayout/typelayout.hpp (主)
            ├── config.hpp
            ├── compile_string.hpp
            ├── type_signature.hpp
            │       └── compile_string.hpp
            ├── reflection_helpers.hpp
            │       └── config.hpp
            ├── signature_generation.hpp
            │       ├── type_signature.hpp
            │       └── reflection_helpers.hpp
            ├── hash.hpp
            ├── verification.hpp
            │       └── hash.hpp
            ├── portability.hpp
            │       └── reflection_helpers.hpp
            └── concepts.hpp
                    ├── signature_generation.hpp
                    └── portability.hpp
```

### 4. 向后兼容性

**现有用法仍然有效**:
```cpp
#include <boost/typelayout.hpp>  // 包含所有功能
```

**新增细粒度包含**:
```cpp
#include <boost/typelayout/hash.hpp>        // 仅哈希功能
#include <boost/typelayout/portability.hpp> // 仅可移植性检查
```

### 5. Include Guard 命名规范

```cpp
#ifndef BOOST_TYPELAYOUT_CONFIG_HPP
#define BOOST_TYPELAYOUT_CONFIG_HPP
// ...
#endif // BOOST_TYPELAYOUT_CONFIG_HPP
```

## Impact
- 代码可读性显著提升
- 便于 Boost 社区审查
- 支持按需包含，减少编译时间
- 完全向后兼容
