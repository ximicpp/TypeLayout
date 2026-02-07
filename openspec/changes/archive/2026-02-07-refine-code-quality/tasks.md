## 1. Implementation

- [x] 1.1 修复 CompileString(string_view) 构造函数：添加 `: value{}` 初始化
- [x] 1.2 在 config.hpp 添加 `has_determinable_layout_v` 类型特征
- [x] 1.3 在 config.hpp 定义统一的 `number_buffer_size` 常量
- [x] 1.4 更新 concepts.hpp：使用 `has_determinable_layout_v`
- [x] 1.5 更新 signature.hpp：为废弃函数添加 `[[deprecated]]` 属性
- [x] 1.6 更新 type_signature.hpp：
  - [x] 1.6.1 合并 prefix lambda 冗余分支
  - [x] 1.6.2 删除 void* 冗余特化
  - [x] 1.6.3 引用 config.hpp 的 number_buffer_size
- [x] 1.7 更新 reflection_helpers.hpp：引用 config.hpp 的 number_buffer_size

## 2. Verification

- [x] 2.1 本地编译测试（P2996 Docker 环境）
- [x] 2.2 确认所有 8 个测试通过