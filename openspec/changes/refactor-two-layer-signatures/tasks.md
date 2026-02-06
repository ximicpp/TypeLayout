# Tasks: refactor-two-layer-signatures

## 1. Core Infrastructure (`config.hpp`)
- [ ] 1.1 替换 `SignatureMode` 枚举：`Physical/Structural/Annotated` → `Layout/Definition`
- [ ] 1.2 更新 `default_signature_mode` 常量（选择 Layout 或 Definition 作为默认）
- [ ] 1.3 版本号升级至 2.0.0

## 2. Definition Signature — 反射引擎 (`reflection_helpers.hpp`)
- [ ] 2.1 实现 `definition_field_signature<T, Index>()` — 单字段带名字签名 `@OFF[name]:TYPE`
- [ ] 2.2 实现 `definition_fields<T>()` — 直接字段列表（含名字）
- [ ] 2.3 实现 `definition_base_signature<T, BaseIndex>()` — 单基类子树 `~base<Name>:record[...]{...}`
- [ ] 2.4 实现 `definition_vbase_signature<T, BaseIndex>()` — 虚基类子树 `~vbase<Name>:record[...]{...}`
- [ ] 2.5 实现 `definition_bases<T>()` — 所有基类子树列表
- [ ] 2.6 实现 `definition_content<T>()` — 组合基类子树 + 直接字段
- [ ] 2.7 实现基类名提取逻辑（使用 `identifier_of` 获取短名）

## 3. Layout Signature — 扁平化引擎 (`reflection_helpers.hpp`)
- [ ] 3.1 保留并优化现有 Physical 模式的扁平化逻辑
- [ ] 3.2 重命名内部函数前缀 `physical_*` → `layout_*`
- [ ] 3.3 确保 Layout 模式字段按偏移排序

## 4. Type Signature 重写 (`type_signature.hpp`)
- [ ] 4.1 class 类型统一使用 `record` 前缀（删除 struct/class 区分逻辑）
- [ ] 4.2 Layout 分支：调用 layout 扁平化引擎，无标记
- [ ] 4.3 Definition 分支：调用 definition 引擎，保留树结构
- [ ] 4.4 Definition 分支：输出 `polymorphic` 标记（有虚函数时）
- [ ] 4.5 Definition 分支：不输出 `inherited` 标记（由 `~base` 隐含）
- [ ] 4.6 确保基本类型（i32、f64、ptr 等）两层签名相同
- [ ] 4.7 确保字节数组归一化在两层中都生效

## 5. Public API 重写 (`signature.hpp`)
- [ ] 5.1 删除所有旧 API 函数
- [ ] 5.2 新增 `get_layout_signature<T>()` — Layout 层签名
- [ ] 5.3 新增 `get_definition_signature<T>()` — Definition 层签名
- [ ] 5.4 新增 `layout_signatures_match<T, U>()` — Layout 签名比较
- [ ] 5.5 新增 `definition_signatures_match<T, U>()` — Definition 签名比较
- [ ] 5.6 新增 `get_layout_hash<T>()` — Layout 层 FNV-1a 哈希
- [ ] 5.7 新增 `get_definition_hash<T>()` — Definition 层哈希
- [ ] 5.8 新增 `layout_hashes_match<T, U>()` — Layout 哈希比较
- [ ] 5.9 新增 `definition_hashes_match<T, U>()` — Definition 哈希比较
- [ ] 5.10 新增 `get_layout_signature_cstr<T>()` — Layout C-string
- [ ] 5.11 新增 `get_definition_signature_cstr<T>()` — Definition C-string
- [ ] 5.12 新增 variable templates: `layout_signature_v<T>`, `definition_signature_v<T>`
- [ ] 5.13 新增 variable templates: `layout_hash_v<T>`, `definition_hash_v<T>`
- [ ] 5.14 新增宏 `TYPELAYOUT_ASSERT_LAYOUT_COMPATIBLE(T1, T2)`
- [ ] 5.15 新增宏 `TYPELAYOUT_ASSERT_DEFINITION_COMPATIBLE(T1, T2)`
- [ ] 5.16 新增宏 `TYPELAYOUT_BIND_LAYOUT(Type, Sig)`
- [ ] 5.17 新增宏 `TYPELAYOUT_BIND_DEFINITION(Type, Sig)`

## 6. Concepts 重写 (`concepts.hpp`)
- [ ] 6.1 删除所有旧 Concepts
- [ ] 6.2 新增 `LayoutSupported<T>` — 类型可分析
- [ ] 6.3 新增 `LayoutCompatible<T, U>` — Layout 签名匹配
- [ ] 6.4 新增 `DefinitionCompatible<T, U>` — Definition 签名匹配
- [ ] 6.5 新增 `LayoutHashCompatible<T, U>` — Layout 哈希匹配
- [ ] 6.6 新增 `DefinitionHashCompatible<T, U>` — Definition 哈希匹配

## 7. Verification 重写 (`verification.hpp`)
- [ ] 7.1 删除旧验证函数
- [ ] 7.2 新增 `get_layout_verification<T>()` — Layout 双哈希验证
- [ ] 7.3 新增 `get_definition_verification<T>()` — Definition 双哈希验证
- [ ] 7.4 新增 `layout_verifications_match<T, U>()`
- [ ] 7.5 新增 `definition_verifications_match<T, U>()`

## 8. Testing — Layout Signature
- [ ] 8.1 测试：简单结构体 Layout 签名格式（`record` 前缀、无名字）
- [ ] 8.2 测试：继承类型 vs 扁平类型 Layout 签名匹配
- [ ] 8.3 测试：多层继承扁平化
- [ ] 8.4 测试：多重继承扁平化
- [ ] 8.5 测试：多态类型 Layout 签名无 `polymorphic` 标记
- [ ] 8.6 测试：字节数组归一化
- [ ] 8.7 测试：位域偏移正确性
- [ ] 8.8 测试：Layout 哈希一致性

## 9. Testing — Definition Signature
- [ ] 9.1 测试：简单结构体 Definition 签名格式（`record` 前缀、含名字）
- [ ] 9.2 测试：基类子树保留 `~base<Name>:record[...]{...}` 格式
- [ ] 9.3 测试：多态类型含 `polymorphic` 标记
- [ ] 9.4 测试：继承类型不含 `inherited` 标记
- [ ] 9.5 测试：匿名成员使用 `<anon:N>` 占位符
- [ ] 9.6 测试：虚基类使用 `~vbase<Name>:` 格式
- [ ] 9.7 测试：Definition 哈希一致性

## 10. Testing — 投影关系验证
- [ ] 10.1 测试：Definition 相同 ⟹ Layout 相同
- [ ] 10.2 测试：Layout 相同但 Definition 不同（继承 vs 扁平）
- [ ] 10.3 测试：Layout 相同但 Definition 不同（字段名不同）
- [ ] 10.4 测试：Layout 不同 ⟹ Definition 不同

## 11. Testing — 回归
- [ ] 11.1 所有基本类型签名正确（i32、f64、ptr 等）
- [ ] 11.2 枚举、联合体、数组签名正确
- [ ] 11.3 嵌套结构体递归签名正确
- [ ] 11.4 GitHub CI 全部通过

## 12. Documentation
- [ ] 12.1 更新 `doc/design/abi-identity.md` — 重写为两层架构
- [ ] 12.2 更新 README — API 参考、模式选择指南、示例代码
- [ ] 12.3 更新 `openspec/project.md` — 项目结构和 API 描述
- [ ] 12.4 更新示例文件 `example/*.cpp`
