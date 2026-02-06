## 1. Implementation

- [x] 1.1 修改 type_signature.hpp：Annotated 模式下保留字节数组元素类型
- [x] 1.2 添加内部包含保护宏：
  - [x] 1.2.1 type_signature.hpp：定义 `BOOST_TYPELAYOUT_INTERNAL_INCLUDE_`
  - [x] 1.2.2 reflection_helpers.hpp：检查保护宏
- [x] 1.3 修改 concepts.hpp：添加 `LayoutSupported` 前置约束
- [x] 1.4 更新 README.md：澄清 Structural 模式的 ABI 布局语义

## 2. Verification

- [ ] 2.1 本地编译测试（P2996 Docker 环境）
- [ ] 2.2 确认所有测试通过