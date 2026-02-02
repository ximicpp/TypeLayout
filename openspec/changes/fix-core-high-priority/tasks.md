## 1. 类型支持扩展
- [x] 1.1 添加 `std::array<T, N>` 特化
  - [x] 1.1.1 在 `type_signature.hpp` 中添加 `#include <array>`
  - [x] 1.1.2 实现 `TypeSignature<std::array<T, N>>` 特化
  - [x] 1.1.3 添加测试用例
- [x] 1.2 添加 `std::pair<T1, T2>` 特化
  - [x] 1.2.1 在 `type_signature.hpp` 中添加 `#include <utility>`
  - [x] 1.2.2 实现 `TypeSignature<std::pair<T1, T2>>` 特化
  - [x] 1.2.3 添加测试用例
- [x] 1.3 添加 `std::span<T, Extent>` 特化 (C++20)
  - [x] 1.3.1 条件包含 `<span>` (检查 `__cpp_lib_span`)
  - [x] 1.3.2 实现 `TypeSignature<std::span<T, Extent>>` 特化
  - [x] 1.3.3 添加测试用例

## 2. 错误诊断增强
- [x] 2.1 改进 static_assert 错误信息
  - [x] 2.1.1 在通用 TypeSignature 中增强错误消息
  - [x] 2.1.2 包含类型类别建议
- [x] 2.2 添加诊断宏和工具
  - [x] 2.2.1 定义 `TypeDiagnostic<T>` 类型诊断辅助类
  - [x] 2.2.2 定义 `TYPELAYOUT_STATIC_ASSERT_SUPPORTED(T, msg)` 宏
  - [x] 2.2.3 添加 `is_layout_supported_v<T>` 变量模板
  - [x] 2.2.4 添加 `LayoutSupported` 概念

## 3. 测试补充
- [x] 3.1 创建 `test_edge_cases.cpp`
  - [x] 3.1.1 零宽度位域测试
  - [x] 3.1.2 超深继承层次测试 (10+ 层)
  - [x] 3.1.3 超大结构体测试 (50+ 字段)
- [x] 3.2 添加 std::array 测试
  - [x] 3.2.1 基本 std::array 签名验证
  - [x] 3.2.2 嵌套 std::array 测试
  - [x] 3.2.3 包含 std::array 成员的结构体

## 4. 文档更新
- [x] 4.1 更新继承文档
  - [x] 4.1.1 添加虚拟继承限制章节
  - [x] 4.1.2 说明签名用于类型识别而非 ABI 保证
- [x] 4.2 更新类型支持文档
  - [x] 4.2.1 添加 std::array 使用示例
  - [x] 4.2.2 添加 std::pair 使用示例
  - [x] 4.2.3 添加 std::span 使用示例
  - [x] 4.2.4 添加 std::atomic 和智能指针文档

## 5. 更新 CMake/Jamfile
- [x] 5.1 添加 test_edge_cases.cpp 到 CMakeLists.txt
- [x] 5.2 添加 test_edge_cases.cpp 到 Jamfile.v2