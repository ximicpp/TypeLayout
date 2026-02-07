# Tasks: refactor-two-layer-signatures

## 1. Core Infrastructure (`config.hpp`)
- [x] 1.1 替换 `SignatureMode` 枚举：`Physical/Structural/Annotated` �?`Layout/Definition`
- [x] 1.2 更新 `default_signature_mode` 常量（选择 Layout �?Definition 作为默认�?
- [x] 1.3 版本号升级至 2.0.0

## 2. Definition Signature �?反射引擎 (`reflection_helpers.hpp`)
- [x] 2.1 实现 `definition_field_signature<T, Index>()` �?单字段带名字签名 `@OFF[name]:TYPE`
- [x] 2.2 实现 `definition_fields<T>()` �?直接字段列表（含名字�?
- [x] 2.3 实现 `definition_base_signature<T, BaseIndex>()` �?单基类子�?`~base<Name>:record[...]{...}`
- [x] 2.4 实现 `definition_vbase_signature<T, BaseIndex>()` �?虚基类子�?`~vbase<Name>:record[...]{...}`
- [x] 2.5 实现 `definition_bases<T>()` �?所有基类子树列�?
- [x] 2.6 实现 `definition_content<T>()` �?组合基类子树 + 直接字段
- [x] 2.7 实现基类名提取逻辑（使�?`identifier_of` 获取短名�?

## 3. Layout Signature �?扁平化引�?(`reflection_helpers.hpp`)
- [x] 3.1 保留并优化现�?Physical 模式的扁平化逻辑
- [x] 3.2 重命名内部函数前缀 `physical_*` �?`layout_*`
- [x] 3.3 确保 Layout 模式字段按偏移排�?

## 4. Type Signature 重写 (`type_signature.hpp`)
- [x] 4.1 class 类型统一使用 `record` 前缀（删�?struct/class 区分逻辑�?
- [x] 4.2 Layout 分支：调�?layout 扁平化引擎，无标�?
- [x] 4.3 Definition 分支：调�?definition 引擎，保留树结构
- [x] 4.4 Definition 分支：输�?`polymorphic` 标记（有虚函数时�?
- [x] 4.5 Definition 分支：不输出 `inherited` 标记（由 `~base` 隐含�?
- [x] 4.6 确保基本类型（i32、f64、ptr 等）两层签名相同
- [x] 4.7 确保字节数组归一化在两层中都生效

## 5. Public API 重写 (`signature.hpp`)
- [x] 5.1 删除所有旧 API 函数
- [x] 5.2 新增 `get_layout_signature<T>()` �?Layout 层签�?
- [x] 5.3 新增 `get_definition_signature<T>()` �?Definition 层签�?
- [x] 5.4 新增 `layout_signatures_match<T, U>()` �?Layout 签名比较
- [x] 5.5 新增 `definition_signatures_match<T, U>()` �?Definition 签名比较
- [x] 5.6 新增 `get_layout_hash<T>()` �?Layout �?FNV-1a 哈希
- [x] 5.7 新增 `get_definition_hash<T>()` �?Definition 层哈�?
- [x] 5.8 新增 `layout_hashes_match<T, U>()` �?Layout 哈希比较
- [x] 5.9 新增 `definition_hashes_match<T, U>()` �?Definition 哈希比较
- [x] 5.10 新增 `get_layout_signature_cstr<T>()` �?Layout C-string
- [x] 5.11 新增 `get_definition_signature_cstr<T>()` �?Definition C-string
- [x] 5.12 新增 variable templates: `layout_signature_v<T>`, `definition_signature_v<T>`
- [x] 5.13 新增 variable templates: `layout_hash_v<T>`, `definition_hash_v<T>`
- [x] 5.14 新增�?`TYPELAYOUT_ASSERT_LAYOUT_COMPATIBLE(T1, T2)`
- [x] 5.15 新增�?`TYPELAYOUT_ASSERT_DEFINITION_COMPATIBLE(T1, T2)`
- [x] 5.16 新增�?`TYPELAYOUT_BIND_LAYOUT(Type, Sig)`
- [x] 5.17 新增�?`TYPELAYOUT_BIND_DEFINITION(Type, Sig)`

## 6. Concepts 重写 (`concepts.hpp`)
- [x] 6.1 删除所有旧 Concepts
- [x] 6.2 新增 `LayoutSupported<T>` �?类型可分�?
- [x] 6.3 新增 `LayoutCompatible<T, U>` �?Layout 签名匹配
- [x] 6.4 新增 `DefinitionCompatible<T, U>` �?Definition 签名匹配
- [x] 6.5 新增 `LayoutHashCompatible<T, U>` �?Layout 哈希匹配
- [x] 6.6 新增 `DefinitionHashCompatible<T, U>` �?Definition 哈希匹配

## 7. Verification 重写 (`verification.hpp`)
- [x] 7.1 删除旧验证函�?
- [x] 7.2 新增 `get_layout_verification<T>()` �?Layout 双哈希验�?
- [x] 7.3 新增 `get_definition_verification<T>()` �?Definition 双哈希验�?
- [x] 7.4 新增 `layout_verifications_match<T, U>()`
- [x] 7.5 新增 `definition_verifications_match<T, U>()`

## 8. Testing �?Layout Signature
- [x] 8.1 测试：简单结构体 Layout 签名格式（`record` 前缀、无名字�?
- [x] 8.2 测试：继承类�?vs 扁平类型 Layout 签名匹配
- [x] 8.3 测试：多层继承扁平化
- [x] 8.4 测试：多重继承扁平化
- [x] 8.5 测试：多态类�?Layout 签名�?`polymorphic` 标记
- [x] 8.6 测试：字节数组归一�?
- [x] 8.7 测试：位域偏移正确�?
- [x] 8.8 测试：Layout 哈希一致�?

## 9. Testing �?Definition Signature
- [x] 9.1 测试：简单结构体 Definition 签名格式（`record` 前缀、含名字�?
- [x] 9.2 测试：基类子树保�?`~base<Name>:record[...]{...}` 格式
- [x] 9.3 测试：多态类型含 `polymorphic` 标记
- [x] 9.4 测试：继承类型不�?`inherited` 标记
- [x] 9.5 测试：匿名成员使�?`<anon:N>` 占位�?
- [x] 9.6 测试：虚基类使用 `~vbase<Name>:` 格式
- [x] 9.7 测试：Definition 哈希一致�?

## 10. Testing �?投影关系验证
- [x] 10.1 测试：Definition 相同 �?Layout 相同
- [x] 10.2 测试：Layout 相同�?Definition 不同（继�?vs 扁平�?
- [x] 10.3 测试：Layout 相同�?Definition 不同（字段名不同�?
- [x] 10.4 测试：Layout 不同 �?Definition 不同

## 11. Testing �?回归
- [x] 11.1 所有基本类型签名正确（i32、f64、ptr 等）
- [x] 11.2 枚举、联合体、数组签名正�?
- [x] 11.3 嵌套结构体递归签名正确
- [x] 11.4 GitHub CI 全部通过

## 12. Documentation
- [x] 12.1 更新 `doc/design/abi-identity.md` �?重写为两层架�?
- [x] 12.2 更新 README �?API 参考、模式选择指南、示例代�?
- [x] 12.3 更新 `openspec/project.md` �?项目结构�?API 描述
- [x] 12.4 更新示例文件 `example/*.cpp`
