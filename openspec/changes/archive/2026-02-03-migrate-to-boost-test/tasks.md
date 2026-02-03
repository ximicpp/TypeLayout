# Tasks: 迁移到 Boost.Test

## 1. 框架配置

- [x] 1.1 添加 Boost.Test 依赖配置 (使用 header-only 模式，无需外部依赖)
- [x] 1.2 更新 `test/Jamfile` 测试构建配置
- [ ] 1.3 更新 CMakeLists.txt 支持 Boost.Test (暂缓，CMake 不是主要构建系统)

## 2. 测试迁移

- [x] 2.1 迁移基础类型测试 (integer_types, floating_point, character_types)
- [x] 2.2 迁移结构体测试 (simple_pod, nested_struct, class_with_private_members)
- [x] 2.3 迁移继承测试 (inheritance)
- [x] 2.4 迁移 bit-field 测试 (bitfields)
- [x] 2.5 迁移 hash 测试 (layout_hash_consistency, different_layouts_different_hashes)

## 3. 扩展测试

- [x] 3.1 添加边界条件测试（empty_struct、aligned_struct）
- [x] 3.2 添加对齐测试（alignas - aligned_struct）
- [x] 3.3 添加 union/enum 类型测试 (union_type, enum_type)
- [x] 3.4 添加标准库类型测试 (optional, variant, tuple)
- [x] 3.5 添加 CV 限定符测试 (cv_qualifiers_stripped)
- [x] 3.6 添加指针/数组类型测试 (pointer_types, array_types)
- [x] 3.7 添加签名匹配测试 (signature_matching)
- [x] 3.8 添加编译期验证测试 (static_assert_abi_guard, layout_verification_struct)

## 4. 验证

- [x] 4.1 所有测试通过 (24 test cases passed)
- [x] 4.2 Docker/WSL 环境验证通过