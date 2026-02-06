# Tasks: add-physical-layout-layer

## 1. Core Infrastructure
- [x] 1.1 `config.hpp`: 在 `SignatureMode` 枚举中添加 `Physical` 值
- [x] 1.2 `compile_string.hpp`: 新增 `skip_first()` 成员函数用于去除前导逗号

## 2. Physical Flattening Module (`reflection_helpers.hpp`)
- [x] 2.1 实现 `physical_field_with_comma<T, Index, OffsetAdj>()` — 单字段签名（逗号前缀）
- [x] 2.2 实现 `physical_direct_fields_prefixed<T, OffsetAdj>()` — 直接字段列表
- [x] 2.3 实现 `physical_one_base_prefixed<T, BaseIndex, OffsetAdj>()` — 单基类扁平化
- [x] 2.4 实现 `physical_bases_prefixed<T, OffsetAdj>()` — 所有基类扁平化
- [x] 2.5 实现 `physical_all_prefixed<T, OffsetAdj>()` — 递归收集所有字段
- [x] 2.6 实现 `get_physical_content<T>()` — 顶层入口，去除前导逗号
- [x] 2.7 处理位域字段的偏移调整

## 3. Type Signature Integration (`type_signature.hpp`)
- [x] 3.1 修改数组 `bytes[]` 归一化条件：`Structural` → `!= Annotated`
- [x] 3.2 class 类型添加 Physical 分支：使用 `record` 前缀，调用 `get_physical_content<T>()`

## 4. Public API (`signature.hpp`)
- [x] 4.1 新增 `get_physical_signature<T>()`
- [x] 4.2 新增 `physical_signatures_match<T1, T2>()`
- [x] 4.3 新增 `get_physical_hash<T>()`
- [x] 4.4 新增 `physical_hashes_match<T1, T2>()`
- [x] 4.5 新增 `get_physical_signature_cstr<T>()`
- [x] 4.6 新增 `physical_signature_v<T>` variable template
- [x] 4.7 新增 `physical_hash_v<T>` variable template
- [x] 4.8 新增 `TYPELAYOUT_ASSERT_PHYSICAL_COMPATIBLE` 宏
- [x] 4.9 新增 `TYPELAYOUT_BIND_PHYSICAL` 宏

## 5. Concepts (`concepts.hpp`)
- [x] 5.1 新增 `PhysicalLayoutCompatible<T, U>` concept
- [x] 5.2 新增 `PhysicalHashCompatible<T, U>` concept

## 6. Verification (`verification.hpp`)
- [x] 6.1 新增 `get_physical_verification<T>()`
- [x] 6.2 新增 `physical_verifications_match<T1, T2>()`

## 7. Testing
- [x] 7.1 测试：继承类型 vs 扁平类型 Physical 签名匹配
- [x] 7.2 测试：多层继承扁平化
- [x] 7.3 测试：多重继承扁平化
- [x] 7.4 测试：无继承结构体两层都匹配
- [x] 7.5 测试：多态类型 Physical 无 polymorphic 标记
- [x] 7.6 测试：Physical 签名格式验证 (`record` 前缀)
- [x] 7.7 测试：向后兼容性（现有 API 默认行为不变）
- [x] 7.8 测试：带位域的继承扁平化
- [x] 7.9 回归测试：所有现有测试通过（GitHub CI）

## 8. Documentation
- [x] 8.1 更新 `doc/design/abi-identity.md` 添加 Physical 层说明
- [x] 8.2 更新 README 说明三层模式架构和使用场景
- [x] 8.3 添加 Physical vs Structural 选择指南
