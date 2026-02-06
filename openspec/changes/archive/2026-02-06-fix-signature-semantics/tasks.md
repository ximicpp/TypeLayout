## 1. 签名模式基础设施
- [x] 1.1 添加 `SignatureMode` 枚举到 `config.hpp`
- [x] 1.2 修改 `get_layout_signature` 接受模式模板参数
- [x] 1.3 实现 `encode_member` 的条件名称包含逻辑

## 2. 签名生成器修改
- [x] 2.1 修改字段编码：Structural 模式省略 `[name]`
- [x] 2.2 修改嵌套类型编码：Structural 模式省略类型名
- [x] 2.3 添加 `get_structural_signature<T>()` 别名
- [x] 2.4 添加 `get_annotated_signature<T>()` 别名

## 3. 哈希和比较修改
- [x] 3.1 确保 `get_layout_hash<T>()` 始终使用 Structural 签名
- [x] 3.2 更新 `signatures_match<T,U>()` 使用 Structural 模式
- [x] 3.3 更新所有概念使用 Structural 模式 (已验证: concepts.hpp 使用 signatures_match)

## 4. 测试用例
- [x] 4.1 测试：相同布局不同名称 → 签名匹配 (test_signature_modes.cpp)
- [x] 4.2 测试：Structural vs Annotated 模式差异 (test_signature_modes.cpp)
- [x] 4.3 测试：嵌套结构体的名称隔离 (test_signature_modes.cpp: OuterA/OuterB)

## 5. 文档更新
- [x] 5.1 更新 README 核心保证表述
- [x] 5.2 添加形式化保证章节 (Signature Modes section in README)
- [x] 5.3 更新 API 文档说明双模式
- [x] 5.4 添加 CHANGELOG 破坏性变更说明