# Prompt：删除 TypeLayout Definition 签名层

---

将以下内容作为后续操作的指令 prompt：

---

## 任务

从 TypeLayout 代码库中完整移除 **Definition 签名模式**，只保留 **Layout 签名模式**。Layout 签名（基于 offset/size/align 的二进制布局指纹）是 TypeLayout 的核心价值，Definition 签名（基于类型名/成员名的人类可读描述）是非核心的调试辅助功能，维护成本高、代码量翻倍，应当移除。

---

## 修改范围与步骤

### 步骤 1：删除 `SignatureMode` 枚举

**文件：`fwd.hpp`（或定义该枚举的头文件）**

```diff
- enum class SignatureMode { Layout, Definition };
```

- 删除整个枚举定义
- 全局搜索 `SignatureMode` 确保无遗漏引用

---

### 步骤 2：精简 `signature_impl.hpp`

删除以下所有 Definition 相关的 constexpr 函数：

```text
删除的函数（精确名称可能有变体，按前缀匹配）：
  ❌ definition_field_signature
  ❌ definition_field_with_comma
  ❌ concatenate_definition_fields
  ❌ definition_fields
  ❌ definition_base_signature
  ❌ definition_base_with_comma
  ❌ concatenate_definition_bases
  ❌ definition_bases
  ❌ definition_content
```

保留的函数（Layout 引擎）：

```text
保留：
  ✅ layout_field_signature / layout_field_with_comma / concatenate_layout_fields / layout_fields
  ✅ layout_base_signature / layout_base_with_comma / concatenate_layout_bases / layout_bases
  ✅ layout_content
```

**重命名**：保留的 `layout_*` 函数去掉 `layout_` 前缀（因为不再有歧义），例如：

```diff
- constexpr auto layout_field_signature(...)
+ constexpr auto field_signature(...)

- constexpr auto layout_fields(...)
+ constexpr auto fields(...)
```

如果重命名风险大、影响面广，也可保留 `layout_` 前缀不改名，优先保证功能正确。

---

### 步骤 3：去除所有函数/类模板中的 `SignatureMode` 模板参数

全局搜索所有带 `SignatureMode Mode` 模板参数的模板，去除该参数：

```diff
- template <typename T, SignatureMode Mode = SignatureMode::Layout>
- struct TypeSignature { ... };
+ template <typename T>
+ struct TypeSignature { ... };

- template <typename T, SignatureMode Mode>
- constexpr auto make_field_sig() { ... }
+ template <typename T>
+ constexpr auto make_field_sig() { ... }
```

内部所有 `if constexpr (Mode == SignatureMode::Definition)` 分支**整体删除**，只保留 `Layout` 分支的逻辑（去掉 `if constexpr` 包裹，直接保留内部代码）。

---

### 步骤 4：精简 `type_map.hpp`

每个 `TypeMap` 特化中，删除 `definition_name()` 方法，只保留 layout 用的名称方法：

```diff
  template<>
  struct TypeMap<int> {
-     static constexpr auto layout_name() { return FixedString("i32"); }
-     static constexpr auto definition_name() { return FixedString("int32_t"); }
+     static constexpr auto name() { return FixedString("i32"); }
  };
```

对所有基础类型特化重复此操作（`int`, `unsigned int`, `float`, `double`, `char`, `bool`, `int8_t`, `int16_t`, `int32_t`, `int64_t`, `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, 指针类型等）。

同时修改所有调用点：`TypeMap<T>::layout_name()` → `TypeMap<T>::name()`。

---

### 步骤 5：精简 `signature.hpp`（公开 API 层）

```diff
- template <typename T>
- constexpr auto get_definition_signature();
  // 删除整个函数

- template <typename T>
- constexpr auto get_layout_signature();
+ template <typename T>
+ constexpr auto get_signature();
  // 保留，可选重命名
```

或者如果 `get_layout_signature` 这个名字已被广泛使用，保持不改也行。

---

### 步骤 6：精简 `sig_types.hpp` / `sig_export.hpp`（导出签名数据结构）

```diff
  template<> struct TypeSig<MyStruct> {
-     static constexpr auto layout_sig     = FixedString("...");
-     static constexpr auto definition_sig = FixedString("...");
+     static constexpr auto sig = FixedString("...");
  };
```

- 删除所有 `definition_sig` 字段
- `layout_sig` 重命名为 `sig`（或保留原名）
- 修改所有读取 `definition_sig` 的调用点

---

### 步骤 7：精简 `compat_check.hpp`

```diff
  struct TypeResult {
      FixedString layout_sig;
-     FixedString definition_sig;
-     bool definition_match;
  };

- // 删除 definition_match() 相关的比较逻辑
```

兼容性检查只基于 layout 签名比对。

---

### 步骤 8：精简导出工具（如 `sig_generator` / CMake 脚本）

如果有导出 `.sig.hpp` 文件的工具代码，删除其中生成 `definition_sig` 字段的逻辑。确保生成的 `.sig.hpp` 只含一个签名字段。

---

### 步骤 9：精简 `opaque.hpp` 中的宏

如果 `TYPELAYOUT_OPAQUE_TYPE` 等宏内部引用了 `definition_name` 或生成了 Definition 签名，删除相关逻辑。

---

### 步骤 10：更新测试

- 删除所有 `Definition` 模式的单元测试用例
- 删除 `SignatureMode::Definition` 相关的测试参数化
- 保留所有 Layout 模式的测试，确保通过
- 如有测试检查 `definition_sig` 字段，改为只检查 `sig` / `layout_sig`

---

### 步骤 11：更新文档

- README / 设计文档中删除 Definition 签名的描述
- 如有 API 文档，删除 `get_definition_signature`、`SignatureMode::Definition` 的条目
- 更新示例代码

---

## 全局搜索检查清单

完成上述步骤后，全局搜索以下关键词，确认零命中（注释和 git 历史除外）：

```text
SignatureMode
Definition
definition_sig
definition_name
definition_field
definition_base
definition_content
get_definition_signature
definition_match
Mode == SignatureMode
```

---

## 不要做的事

1. **不要修改 Layout 签名的生成逻辑** —— 它是核心，只删 Definition，不改 Layout
2. **不要改变 Layout 签名的字符串格式** —— 已导出的 `.sig.hpp` 要保持兼容
3. **不要删除 `FixedString`** —— 它仍被 Layout 签名使用
4. **不要删除 P2996 反射相关代码（`reflect.hpp`）** —— 它同时服务于 Layout 签名
5. **不要删除 `type_map.hpp`** —— 它仍提供基础类型到 Layout 签名标识符的映射

---

## 预期效果

| 指标 | 删除前 | 删除后 |
|------|--------|--------|
| `signature_impl.hpp` 行数 | ~N | ~N/2 |
| `type_map.hpp` 每个特化的方法数 | 2 | 1 |
| 模板实例化数量（每个用户类型） | 2 | 1 |
| 签名引擎函数总数 | ~18 | ~9 |
| `SignatureMode` 模板参数传染 | 全局 | 消除 |
| 编译时间 | 基准 | 预期减少 20-40% |
| 导出 `.sig.hpp` 文件体积 | 基准 | 约减半 |