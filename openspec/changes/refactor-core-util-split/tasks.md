# Tasks: Core/Util 分层架构重构

## 1. 目录结构重组
- [x] 1.1 创建 `include/boost/typelayout/core/` 目录
- [x] 1.2 创建 `include/boost/typelayout/util/` 目录
- [x] 1.3 移动核心文件到 `core/`：
  - [x] `detail/type_signature.hpp` → `core/type_signature.hpp`
  - [x] `detail/compile_string.hpp` → `core/compile_string.hpp`
  - [x] `detail/hash.hpp` → `core/hash.hpp`
  - [x] `detail/config.hpp` → `core/config.hpp`
  - [x] `detail/reflection_helpers.hpp` → `core/reflection_helpers.hpp`
  - [x] 新建 `core/signature.hpp` (公共 API)
  - [x] 新建 `core/verification.hpp`
  - [x] 新建 `core/concepts.hpp` (核心布局概念)
- [x] 1.4 移动实用工具文件到 `util/`：
  - [x] `detail/serialization_status.hpp` → `util/serialization_check.hpp`
  - [x] `detail/serialization_traits.hpp` → `util/platform_set.hpp`
  - [x] 新建 `util/concepts.hpp` (序列化概念)
- [x] 1.5 更新/创建顶层头文件：
  - [x] `typelayout.hpp` - 仅包含 core
  - [x] `typelayout_util.hpp` - 包含 util（依赖 core）
  - [x] `typelayout_all.hpp` - 包含全部（便捷头文件）

## 2. 兼容层
- [x] 2.1 创建旧路径的兼容头文件（deprecated warnings）
- [x] 2.2 在兼容头文件中添加 `#warning` / `#pragma message` 提示
- [x] 2.3 更新 `signature.hpp` 为兼容头文件
- [x] 2.4 更新 `verification.hpp` 为兼容头文件
- [x] 2.5 更新 `concepts.hpp` 为兼容头文件
- [x] 2.6 更新 `portability.hpp` 为兼容头文件

## 3. 内部依赖更新
- [x] 3.1 更新 `core/` 内部的 `#include` 路径
- [x] 3.2 更新 `util/` 内部的 `#include` 路径
- [x] 3.3 确保 `util/` 正确依赖 `core/`
- [x] 3.4 更新 `fwd.hpp` 包含 core 和 util 前向声明

## 4. 文档更新
- [x] 4.1 更新 `README.md` - 强调核心价值定位、更新项目结构
- [x] 4.2 更新 `doc/quickstart.md` - 分别介绍 core 和 util
- [x] 4.3 更新 `doc/api_reference.md` - 按 core/util 分类
- [x] 4.4 更新 `doc/technical_overview.md` - 调整架构叙事
- [x] 4.5 更新 Antora 文档 `doc/modules/ROOT/pages/` (overview, quickstart)

## 5. 示例更新
- [x] 5.1 创建 `example/core_demo.cpp` - 纯核心功能演示
- [x] 5.2 创建 `example/util_demo.cpp` - 实用工具演示
- [x] 5.3 更新现有示例 `demo.cpp` 的 `#include` 路径

## 6. 测试更新
- [x] 6.1 分离核心测试和实用工具测试（测试文件使用 typelayout_all.hpp 覆盖全部功能）
- [x] 6.2 更新测试文件的 `#include` 路径

## 7. OpenSpec 规范更新
- [x] 7.1 更新 `signature` spec - 标记为核心
- [x] 7.2 更新 `portability` spec - 标记为实用工具
- [x] 7.3 更新 `project.md` - 反映新目录结构

## 8. 验证
- [x] 8.1 编译测试
  - 使用 Clang P2996 21.0.0 成功构建所有目标
  - 所有测试通过：test_all_types, test_serialization_signature, test_anonymous_member, test_signature_extended, test_signature_comprehensive
  - 所有示例运行正常：core_demo, util_demo, demo, shared_memory_demo, zero_copy_network_demo, network_protocol_example, file_format_example
- [x] 8.2 确保旧 include 路径仍可用（带警告）
  - 兼容头文件已创建在 `include/boost/typelayout/` 顶层
  - 包含 `#pragma message` 废弃提示
- [x] 8.3 确保新 include 路径工作正常
  - `typelayout.hpp` - 核心功能 ✅
  - `typelayout_util.hpp` - 实用工具 ✅
  - `typelayout_all.hpp` - 完整库 ✅
