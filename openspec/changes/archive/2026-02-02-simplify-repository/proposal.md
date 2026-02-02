# Change: 精简仓库，聚焦核心 TypeLayout 兼容性功能

## Why
仓库中存在大量废弃的兼容性头文件（`detail/` 目录和根目录的旧 API），
这些文件只是重定向到新位置，增加了维护成本和新用户的理解负担。

精简后的仓库结构更清晰，更能体现 TypeLayout 的核心价值：
**为零拷贝二进制数据交换提供类型安全保障**

## What Changes

### 移除的文件 (11个)

#### detail/ 目录 (全部移除)
- `detail/compile_string.hpp` - 重定向到 core/
- `detail/config.hpp` - 重定向到 core/
- `detail/hash.hpp` - 重定向到 core/
- `detail/reflection_helpers.hpp` - 重定向到 core/
- `detail/serialization_status.hpp` - 废弃
- `detail/serialization_traits.hpp` - 废弃
- `detail/type_signature.hpp` - 重定向到 core/

#### 根目录废弃文件 (4个)
- `signature.hpp` - 重定向到 core/signature.hpp
- `verification.hpp` - 重定向到 core/verification.hpp
- `concepts.hpp` - 重定向到 core/ 和 util/
- `portability.hpp` - 重定向到 util/

### 保留的文件

```
include/boost/typelayout/
├── core/                      # 核心层 (8个文件)
│   ├── compile_string.hpp
│   ├── concepts.hpp
│   ├── config.hpp
│   ├── hash.hpp
│   ├── reflection_helpers.hpp
│   ├── signature.hpp
│   ├── type_signature.hpp
│   └── verification.hpp
├── util/                      # 工具层 (3个文件)
│   ├── concepts.hpp
│   ├── platform_set.hpp
│   └── serialization_check.hpp
├── typelayout.hpp             # 核心层入口
├── typelayout_util.hpp        # 工具层入口
├── typelayout_all.hpp         # 完整功能入口
├── fwd.hpp                    # 前向声明
└── compat.hpp                 # 跨平台工具
```

## Impact
- **无破坏性变更**: 废弃文件没有被任何测试使用
- 如果有外部用户依赖废弃 API，需要更新 include 路径
- 文档需要更新以反映新结构

## 分支
`feature/simplify-repository`
