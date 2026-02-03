# Change: Analyze User-Defined Types Support

## Why

TypeLayout 声称支持"所有 C++ 类型"，但用户自定义类型（class/struct）场景复杂多样，需要系统性验证：

1. **各种 class 变体** - 是否支持所有 class 的布局场景？
2. **特殊成员** - static 成员、const 成员、mutable 成员如何处理？
3. **访问控制** - private/protected 成员是否正确反射？
4. **模板类** - 模板实例化后的类型是否支持？
5. **嵌套类型** - 嵌套 class/struct/enum 是否支持？

## What Changes

这是一个**分析性 proposal**，验证用户自定义类型的支持情况：

- 创建测试用例覆盖所有 class 变体
- 验证签名生成的正确性
- 记录任何限制或边界情况
- 产出用户自定义类型支持矩阵

## Impact

- Affected specs: signature/spec.md (可能需要补充文档)
- Test files: 新增 test_user_defined_types.cpp
- Documentation: 更新类型支持文档

---

## Analysis Summary (分析摘要)

### 结论: ⭐⭐⭐⭐⭐ (5/5) - 完全支持

TypeLayout **完全支持所有用户自定义类型**。

### 支持矩阵

| 类别 | 覆盖项 | 状态 |
|------|--------|------|
| **类变体** | 空类、POD、Standard Layout、非平凡类、抽象类、Final 类 | ✅ 全部 |
| **成员类型** | static (排除)、const、mutable、引用、指针、数组、位域 | ✅ 全部 |
| **访问控制** | public、protected、private | ✅ 全部 |
| **模板类型** | 简单模板、特化、部分特化、变参模板、CRTP | ✅ 全部 |
| **嵌套类型** | 嵌套 struct/class/enum/union、匿名嵌套 | ✅ 全部 |
| **继承** | 单继承、多继承、虚拟继承、钻石继承、私有继承 | ✅ 全部 |
| **特殊情况** | EBO、[[no_unique_address]]、alignas、packed、union 成员 | ✅ 全部 |

### 关键实现

P2996 反射使用 `access_context::unchecked()` 确保：
- ✅ 访问所有访问级别的成员
- ✅ 正确排除 static 成员
- ✅ 反射所有非静态数据成员

### 新增测试

创建了 `test/test_user_defined_types.cpp` 覆盖 38 个测试场景。

详细分析见 `tasks.md`
