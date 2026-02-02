
# Change: 聚焦核心功能 - 精简非核心代码和文档

## Why

经过工具层移除后，TypeLayout 已经是一个纯核心层库。但仍存在以下问题：

1. **OpenSpec 规范文件冗余** - 存在 11 个 spec 文件，很多已过时或与移除的工具层相关
2. **测试文件冗余** - 部分测试文件与核心功能重叠或针对已移除的功能
3. **示例文件过多** - 7 个示例文件中有些过于复杂或重复
4. **文档结构复杂** - doc/ 目录结构臃肿，包含过时内容
5. **工具文件冗余** - tools/ 目录中的工具与核心功能关系不明确

## What Changes

### 1. 清理 OpenSpec 规范 (specs/)

删除过时或不相关的规范文件：
- `openspec/specs/compat-tool/` - 兼容性工具已移除
- `openspec/specs/design-issues/` - 设计问题分析，不需要长期保留
- `openspec/specs/limitations/` - 限制分析，可合并到其他文档
- `openspec/specs/analysis/` - 分析类规范，不需要长期保留
- `openspec/specs/portability/` - 可移植性规范，与移除的工具层相关
- `openspec/specs/use-cases/` - 用例分析，可移至文档

保留核心规范：
- `openspec/specs/signature/` - 核心签名功能规范
- `openspec/specs/repository-structure/` - 仓库结构（需更新）
- `openspec/specs/build-system/` - 构建系统（需更新）
- `openspec/specs/ci/` - CI 配置（需更新）
- `openspec/specs/documentation/` - 文档规范（需更新）

### 2. 精简测试文件 (test/)

当前测试文件：
- `test_all_types.cpp` - 综合测试 ✅ 保留
- `test_signature_comprehensive.cpp` - 签名综合测试 ✅ 保留
- `test_signature_extended.cpp` - 扩展签名测试 ✅ 保留
- `test_anonymous_member.cpp` - 匿名成员测试 ✅ 保留
- `test_primitives.cpp` - 基本类型测试，与 test_all_types 重叠
- `test_structs.cpp` - 结构体测试，与 test_all_types 重叠
- `test_inheritance.cpp` - 继承测试，与 test_all_types 重叠
- `test_bitfields.cpp` - 位域测试，与 test_all_types 重叠
- `test_concepts.cpp` - 概念测试，与 test_all_types 重叠
- `test_hash.cpp` - 哈希测试，与 test_all_types 重叠
- `test_edge_cases.cpp` - 边缘情况测试，与 test_all_types 重叠
- `test_portability.cpp` - 可移植性测试，与移除的工具层相关

### 3. 精简示例文件 (example/)

当前示例文件：
- `demo.cpp` - 基础演示 ✅ 保留
- `core_demo.cpp` - 核心功能演示 ✅ 保留（可考虑合并到 demo.cpp）
- `network_protocol.cpp` - 网络协议示例 ✅ 保留
- `file_format.cpp` - 文件格式示例 ✅ 保留
- `shared_memory_demo.cpp` - 共享内存示例 ✅ 保留
- `zero_copy_network_demo.cpp` - 零拷贝网络示例 ⚠️ 与 network_protocol 重叠
- `xoffset_demo.cpp` - X 偏移演示 ⚠️ 考虑删除或合并

### 4. 精简文档目录 (doc/)

删除过时内容：
- `doc/build/` - 构建产物，应在 .gitignore
- `doc/test/` - 测试产物
- `doc/reports/` - 报告

更新内容：
- `doc/api_reference.md` - API 参考文档
- `doc/quickstart.md` - 快速入门

### 5. 精简工具目录 (tools/)

- `tools/typelayout_tool.cpp` - CLI 工具 ✅ 保留
- `tools/compare_signatures.cpp` - 签名比较工具 ⚠️ 考虑合并
- `tools/siggen.cpp` - 签名生成工具 ⚠️ 考虑合并
- `tools/demo_tool.sh` - 演示脚本 ⚠️ 考虑删除

### 6. 其他清理

- 更新 `README.md` 确保描述准确
- 更新 `CMakeLists.txt` 移除删除文件的引用
- 清理根目录文件

## Impact

- Affected specs: repository-structure, build-system, ci, documentation
- Affected code: test/, example/, tools/, doc/
- **BREAKING**: 无 - 不影响核心头文件
