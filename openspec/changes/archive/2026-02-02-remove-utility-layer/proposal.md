# Change: 移除工具层，专注核心签名功能

## Why

TypeLayout 库的核心价值是**编译时类型布局签名**——这是唯一无法被其他库替代的独特能力。
工具层 (`util/`) 提供的序列化检查功能虽然有用，但：
1. 与核心签名功能存在概念重叠
2. 增加了维护负担和学习成本
3. 用户可以基于核心 API 自行实现

移除工具层可以让库更加聚焦，降低复杂度。

## What Changes

**移除文件 (7 个):**
- `include/boost/typelayout/util/` 整个目录 (3 个文件)
  - `platform_set.hpp` - PlatformSet 约束
  - `serialization_check.hpp` - is_serializable_v
  - `concepts.hpp` - Serializable, ZeroCopyTransmittable
- `include/boost/typelayout/typelayout_util.hpp` - 工具层入口
- `include/boost/typelayout/typelayout_all.hpp` - 全功能入口
- `include/boost/typelayout/compat.hpp` - CI 兼容工具
- `include/boost/typelayout/fwd.hpp` - 前向声明

**保留文件 (9 个):**
- `include/boost/typelayout/typelayout.hpp` - 唯一入口
- `include/boost/typelayout/core/` 整个目录 (8 个文件)
  - `compile_string.hpp`
  - `config.hpp`
  - `hash.hpp`
  - `reflection_helpers.hpp`
  - `type_signature.hpp`
  - `signature.hpp`
  - `verification.hpp`
  - `concepts.hpp`

**更新文件:**
- `typelayout.hpp` - 移除对 util/ 的可选 include
- `README.md` - 更新项目结构和 API 文档
- 测试文件 - 移除对工具层的测试

## Impact

- 受影响的规格: `repository-structure`
- 代码量减少: 约 40% 的头文件
- API 减少: 移除 `is_serializable_v`, `PlatformSet`, `Serializable` 概念
- **BREAKING**: 依赖 `typelayout_util.hpp` 或 `typelayout_all.hpp` 的代码将无法编译
