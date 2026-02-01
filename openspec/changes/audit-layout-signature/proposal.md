# Change: Audit Layout Signature Completeness

## Why

布局签名系统是 TypeLayout 库的核心功能。当前实现需要全面审计，以确保：
1. 所有 C++ 类型都能正确生成签名
2. 边界情况和复杂类型能正确处理
3. 签名准确反映实际内存布局

## What Changes

### 审计范围

1. **基础类型覆盖检查**
   - 所有固定宽度整数类型 (int8_t ~ int64_t)
   - 浮点类型 (float, double, long double)
   - 字符类型 (char, wchar_t, char8_t, char16_t, char32_t)
   - 布尔和特殊类型 (bool, std::byte, std::nullptr_t)

2. **复合类型检查**
   - 指针类型 (`T*`, `void*`, 函数指针)
   - 引用类型 (`T&`, `T&&`)
   - 成员指针 (`T C::*`)
   - 数组类型 (`T[N]`, 多维数组)
   - CV 限定类型 (const, volatile)

3. **用户定义类型检查**
   - 空结构体
   - 简单 POD 结构体
   - 带继承的类（单继承、多继承、虚继承）
   - 多态类（带 virtual 函数）
   - 联合体 (union)
   - 枚举类型 (enum, enum class)
   - 位域

4. **标准库类型检查**
   - 智能指针 (unique_ptr, shared_ptr, weak_ptr)
   - 可选类型 (std::optional)
   - 变体类型 (std::variant)
   - 元组 (std::tuple)
   - 容器类型 (考虑是否应该支持)

5. **边界情况检查**
   - 嵌套类型
   - 匿名联合/结构体
   - 零大小类型 (`[[no_unique_address]]`)
   - 对齐属性 (`alignas`)
   - packed 属性

## Impact

- 影响代码: `include/boost/typelayout/detail/type_signature.hpp`
- 可能需要新增: 测试文件 `test/test_signature_comprehensive.cpp`
- 可能需要更新: 文档说明支持的类型范围

## 审计方法

1. 为每种类型创建测试用例
2. 验证签名能成功生成
3. 验证签名格式正确
4. 验证签名能区分不同布局的类型
