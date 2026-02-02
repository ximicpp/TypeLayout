# 任务清单: 聚焦核心功能精简

## 1. 清理 OpenSpec 规范文件

- [x] 1.1 删除 `openspec/specs/compat-tool/` 目录
- [x] 1.2 删除 `openspec/specs/design-issues/` 目录
- [x] 1.3 删除 `openspec/specs/limitations/` 目录
- [x] 1.4 删除 `openspec/specs/analysis/` 目录
- [x] 1.5 删除 `openspec/specs/portability/` 目录
- [x] 1.6 删除 `openspec/specs/use-cases/` 目录
- [x] 1.7 更新 `openspec/specs/repository-structure/spec.md` (通过 delta 文件)
- [x] 1.8 更新 `openspec/specs/signature/spec.md` (保持现状)

## 2. 精简测试文件

- [x] 2.1 删除 `test/test_primitives.cpp` (与 test_all_types 重叠)
- [x] 2.2 删除 `test/test_structs.cpp` (与 test_all_types 重叠)
- [x] 2.3 删除 `test/test_inheritance.cpp` (与 test_all_types 重叠)
- [x] 2.4 删除 `test/test_bitfields.cpp` (与 test_all_types 重叠)
- [x] 2.5 删除 `test/test_concepts.cpp` (与 test_all_types 重叠)
- [x] 2.6 删除 `test/test_hash.cpp` (与 test_all_types 重叠)
- [x] 2.7 删除 `test/test_edge_cases.cpp` (与 test_all_types 重叠)
- [x] 2.8 删除 `test/test_portability.cpp` (工具层相关)
- [x] 2.9 删除 `test/Jamfile.v2`, `test/run_extended_tests.*`
- [x] 2.10 CMakeLists.txt 已正确配置 4 个测试

## 3. 精简示例文件

- [x] 3.1 删除 `example/xoffset_demo.cpp`
- [x] 3.2 删除 `example/zero_copy_network_demo.cpp` (与 network_protocol 重叠)
- [x] 3.3 删除 `example/core_demo.cpp` (合并入 demo.cpp 概念)
- [x] 3.4 删除 `example/typelayout.config.example.hpp`
- [x] 3.5 更新根 `CMakeLists.txt` ✅

## 4. 精简文档目录

- [x] 4.1 删除 `doc/build/` 目录
- [x] 4.2 删除 `doc/test/` 目录
- [x] 4.3 删除 `doc/reports/` 目录
- [x] 4.4 删除 `doc/api/` 目录
- [x] 4.5 更新 `.gitignore` 忽略构建产物

## 5. 精简工具目录

- [x] 5.1 删除 `tools/compare_signatures.cpp`
- [x] 5.2 删除 `tools/siggen.cpp`
- [x] 5.3 删除 `tools/demo_tool.sh`
- [x] 5.4 CMakeLists.txt 已正确配置

## 6. 清理根目录

- [x] 6.1 删除 `build_wsl/` 目录 (构建产物)
- [x] 6.2 删除 `-p/` 目录 (误创建)
- [x] 6.3 删除 `docs/` 目录 (重复的文档目录)
- [x] 6.4 删除 `package.json` (不需要 npm)
- [x] 6.5 删除 `antora-playbook.yml` (根目录不需要)
- [x] 6.6 删除 `build.jam` (不使用 B2 构建)
- [x] 6.7 删除 `build_and_run.sh` (使用 Docker 替代)
- [x] 6.8 更新 `.gitignore`

## 7. 验证

- [x] 7.1 构建测试 ✅ 100% 成功
- [x] 7.2 运行测试套件 ✅ 4/4 通过
- [x] 7.3 确认核心功能正常 ✅