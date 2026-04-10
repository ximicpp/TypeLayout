# Conference Proposal 逐行审计报告

审计对象: `docs/conference-proposal-draft.md`
审计方法: 将提案中每一条事实性声明与源代码逐一交叉验证
审计日期: 2026-04-10

---

## 第 5 行 — 标题

> "Can I Safely memcpy This Type? Verifying Layout at Compile Time with C++26 Reflection"

**准确。** `is_byte_copy_safe_v<T>`（admission.hpp）与 `get_layout_signature<T>()`（signature.hpp）均为 `consteval`，基于 P2996 实现。标题精确概括了库的核心能力。

---

## 第 13 行 — 摘要第一段

> "yet the language gives us little build-time proof that two targets agree on byte-level layout"

**准确。** 现有标准工具（`sizeof`、`offsetof`、`alignof`、`is_trivially_copyable`）各自只能覆盖问题的一个侧面，均无法提供完整的字节级布局证明。

> "A type can be trivially copyable and still be unsafe to transport"

**准确。** 例如 `struct { int* p; }` 满足 `trivially_copyable`，但 `is_byte_copy_safe_v` 返回 `false`。验证路径：admission.hpp 分支 2 要求 `!has_pointer`，而签名中包含 `ptr[` 令牌，被 `sig_has_pointer()` 捕获。

> "a type that looks harmless in source can still compile to a different representation on another platform"

**准确。** 代码中的具体实现：
- `long` → 依据 `sizeof(long)` 映射为 `i32` 或 `i64`（type_map.hpp:42-55）
- `wchar_t` → Windows 上 `wchar[s:2,a:2]`，Linux 上 `wchar[s:4,a:4]`（type_map.hpp:117）
- `long double` → 通过 `__LDBL_MANT_DIG__` 映射为 `fld64`/`fld80`/`fld106`/`fld128`（config.hpp:79-95）

---

## 第 15 行 — 摘要第二段

> "If the compiler can enumerate fields, base classes, offsets, and bit-fields"

**准确。** reflect.hpp 使用 `nonstatic_data_members_of`、`bases_of`；signature_impl.hpp 使用 `offset_of`、`is_bit_field`、`bit_size_of`。四项反射能力均有对应代码。

> "it can derive a canonical signature"

**准确。** `get_layout_signature<T>()`（signature.hpp:31）为 `consteval`，返回 `FixedString`。对于给定类型和平台，签名是确定性的。

> "That same machinery, together with a small set of explicit safety rules"

**准确。** "规则"对应 admission.hpp 的四个分支：
1. 不透明类型：检查 `!has_pointer && opaque_copy_safe<T>::value`
2. `trivially_copyable` + `!has_pointer` 快速路径
3. 类/联合体：递归检查成员与基类
4. 其余一律拒绝

> "generate signatures on target platforms, compare them in a verification build, and wire the checks into CI"

**准确。**
- 生成：`SigExporter` + `TYPELAYOUT_EXPORT_TYPES`（sig_export.hpp）生成 `.sig.hpp`
- 比较：`compat::layout_match()`（compat_check.hpp:31）为 `constexpr`，可用于 `static_assert`
- CI 集成：`CompatReporter::all_types_transfer_safe()` 提供运行时布尔值作为 CI 退出码

> "three concrete examples: a fixed-width type that passes, a type whose layout matches but still contains unsafe pointers, and a type that diverges across platforms"

**准确（作为演讲规划内容）。** 三种场景均可通过库的机制表达：
- 定宽类型通过：`struct { uint32_t a; double b; }` → 签名匹配且 `is_byte_copy_safe_v = true`
- 布局匹配但含不安全指针：`struct { int* p; int x; }` → 签名可能匹配但 `is_byte_copy_safe_v = false`
- 跨平台分歧：含 `long` 的结构体 → ILP32 与 LP64 签名不同

---

## 第 17 行 — 摘要第三段

> "it proves layout agreement and checks stated transport-safety assumptions; it does not prove semantic compatibility"

**准确。** CLAUDE.md 明确记载："Design trade-off: correctly answers 'can I memcpy these bytes?' but cannot detect semantic field reordering."

> "virtual inheritance, opaque types, and implementation-defined fields such as `long`, `wchar_t`, and `long double`"

**准确。**
- 虚继承：signature_impl.hpp:190-192 以 `static_assert` 在编译期拒绝
- 不透明类型：opaque.hpp 中的 `TYPELAYOUT_REGISTER_OPAQUE` 系列宏
- 实现定义字段：type_map.hpp + config.hpp 中的平台相关编码

---

## 第 21–23 行 — 关键收获

> "why `trivially_copyable` and `sizeof` checks are not enough"

**准确。** `trivially_copyable` 不检测指针；`sizeof` 不检测字段级布局。库的存在意义即弥补此差距。

> "without an external IDL, generated serialization stubs, or runtime verification overhead"

**准确。**
- 无外部 IDL：直接作用于原生 C++ 类型
- 无序列化桩代码：`.sig.hpp` 是签名数据头文件，不生成序列化/反序列化代码
- 无运行时验证开销：核心签名生成与比较均为 `consteval`/`constexpr`，零运行时开销。`CompatReporter` 为可选的运行时便利工具

> "export signatures per platform, aggregate them in a verification build, and fail the build on layout mismatches"

**准确。** `.sig.hpp` 导出为 `inline constexpr const char[]` 数组，可包含并用于 `static_assert`。

---

## 第 29–31 行 — 第 1 节大纲

> "The two recurring failure modes: transport-unsound bytes and cross-platform representation drift"

**准确。** 对应库的两个核心检查：
- 传输不安全字节 → `is_byte_copy_safe_v<T>` 检测
- 跨平台表示漂移 → 签名比较检测

> "Why existing checks -- `trivially_copyable`, `sizeof`, and even IDL-based approaches -- each address only part of the problem"

**准确。** 库的设计出发点正是现有方案各有盲区。

---

## 第 35–38 行 — 第 2 节大纲

> "Using reflection to enumerate fields, bases, offsets, and bit-fields"

**准确。** 逐项验证：
- 字段：`nonstatic_data_members_of`（reflect.hpp:25）
- 基类：`bases_of`（reflect.hpp:30）
- 偏移量：`offset_of`（signature_impl.hpp:116-117, 133, 146）
- 位域：`is_bit_field`（signature_impl.hpp:115）、`bit_size_of`（signature_impl.hpp:119）

> "Recursively flattening nested structs and base classes into a canonical compile-time representation with absolute offsets"

**准确。** 代码验证：
- 嵌套结构体展平：signature_impl.hpp:130-134，非空类字段递归调用 `layout_all_prefixed<FieldType, field_offset>()`
- 基类展平：signature_impl.hpp:177-178，同理
- 绝对偏移量：`OffsetAdj` 模板参数逐层累加父级偏移
- 编译期：所有函数均为 `consteval`
- 规范性：对于给定类型 + 平台，输出确定唯一

> 示例签名: `[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:f64[s:8,a:8]}`

**已通过代码追踪验证。** 对 `struct { uint32_t a; double b; }` 在 64 位小端平台上逐步追踪：
1. `get_arch_prefix()` → `sizeof(void*) == 8` + 小端 → `[64-le]`（signature.hpp:18-19）
2. 主模板 → `std::is_class_v` 为真，非多态 → `record[s:16,a:8]{...}`（type_map.hpp:263-269）
3. 字段 0：`uint32_t`，偏移 0 → `@0:u32[s:4,a:4]`（type_map.hpp:97）
4. 字段 1：`double`，偏移 8（4 字节对齐填充后）→ `@8:f64[s:8,a:8]`（type_map.hpp:107）
5. `get_layout_content<T>()` 调用 `skip_first()` 去掉前导逗号 → 最终结果完全匹配

> "Edge cases such as bit-fields, arrays, and empty bases"

**准确。**
- 位域：signature_impl.hpp:115-128 → `@byte.bit:bits<width,type>` 格式
- 数组：type_map.hpp:213-225 → `array[s:S,a:A]<elem,count>` 或字节数组 `bytes[s:N,a:1]`
- 空基类：signature_impl.hpp:165-170 + `embedded_empty_signature<>()`（85-103 行）将 `s:1` 改写为 `s:0`

---

## 第 42–45 行 — 第 3 节大纲

> "Layout agreement: comparing signatures across platforms"

**准确。** `compat::layout_match()`（compat_check.hpp:31）为 `constexpr`，直接比较签名字符串。

> "Transport-safety checks: combining the signature with explicit rules for pointer-like and polymorphic cases, plus explicit contracts for opaque types"

**准确。**
- 指针类检测：`sig_has_pointer()` 扫描 `ptr[`、`fnptr[`、`memptr[`、`ref[`、`rref[`、`vptr` 令牌（fixed_string.hpp:232-239，sig_parser.hpp:37-44）
- 多态类型：签名中 `vptr` 标记（type_map.hpp:259），`SigExporter::add<T>()` 以 `static_assert` 阻止非 `trivially_copyable` 类型导出（sig_export.hpp:59-63）
- 不透明合约：`opaque_copy_safe<T>` 默认特征（fwd.hpp:34-35），由注册宏特化（opaque.hpp:49, 79 等）

> "Cross-platform comparison: using signatures to expose implementation-defined differences"

**准确。** `classify_signature()`（safety_level.hpp:46-67）将含 `wchar[`、`fld*[`、`bits<` 的签名分类为 `PlatformVariant`，在报告中显式标出。

> "Three concrete examples"

**准确（作为演讲内容）。** 三种场景均在库能力范围内。

---

## 第 49–52 行 — 第 4 节大纲

> "Each target platform exports `.sig.hpp` headers for a selected set of boundary types"

**准确。** `SigExporter::write()`（sig_export.hpp:99-117）生成含 include guard、命名空间、constexpr 数据的 `.sig.hpp`。`TYPELAYOUT_EXPORT_TYPES` 宏（sig_export.hpp:280-295）生成 `main()`。"selected set" 对应用户通过 `add<T>()` 显式注册的类型。

> "A verification build on any single platform `#include`s all exported headers and runs `static_assert` on signature equality"

**准确。** `.sig.hpp` 中的数据为 `inline constexpr const char[]`，`layout_match()` 为 `constexpr`，二者配合可直接用于 `static_assert`。

> "show reporter output that pinpoints which types and which fields differ"

### [已修复] 原问题 — 措辞过度，现已通过代码实现对齐

**原审计发现：** `CompatReporter` 原先只输出签名字符串中首个分歧字符的位置（`format_diff` 函数），并不展示"哪个字段不一致"。由于签名是无字段名的，审计建议修改提案措辞。

**采取的措施：** 改为增强代码以兑现提案原意。`compat_check.hpp` 新增：

1. `detail::SigField`、`make_sig_field()`、`sig_header()`、`parse_sig_fields()` — 括号深度感知的签名字段解析器（支持 `array<...>`、`O(tag|N|A)<...>` 等嵌套结构）。
2. `CompatReporter::format_field_diff()` — 按位置比较字段列表，只列出差异字段，并标注 header（`[ARCH]record[s:S,a:A]`）是否不同。
3. `print_diff_report()` 在现有 `format_diff` 字符级位置标注之后追加字段级差异输出。

**输出示例**（来自独立验证程序）：

```
[DIFFER] UnsafeStruct layout signatures:
  x86_64_linux_clang : [64-le]record[s:48,a:16]{@0:i64[s:8,a:8],@8:ptr[s:8,a:8],@16:wchar[s:4,a:4],@32:fld80[s:16,a:16]}
  arm64_macos_clang  : [64-le]record[s:32,a:8]{@0:i64[s:8,a:8],@8:ptr[s:8,a:8],@16:wchar[s:4,a:4],@24:fld64[s:8,a:8]}
                                        ^--- diverges at position 16 ('4' vs '3')
    Field diff: 1 of 4 field(s) differ; header differs
      header: [64-le]record[s:48,a:16]
          vs: [64-le]record[s:32,a:8]
      #4: @32:fld80[s:16,a:16] vs @24:fld64[s:8,a:8]
```

**结论：** 提案第 51 行"pinpoints which types and which fields differ"的措辞现已被代码兑现，无需修改提案文本。字段索引使用 `#N`（而非字段名，因签名层故意无名），但已足以让开发者定位到具体字段。

---

## 第 56–57 行 — 第 5 节大纲

> "Handling virtual inheritance"

**准确。** signature_impl.hpp:190-192 以 `static_assert` 在编译期拒绝。`has_virtual_base<T>()`（reflect.hpp:34-54）递归遍历整个基类层次结构，检测所有深度的虚继承。

> "explicitly-registered opaque types"

**准确。** opaque.hpp 提供四个注册宏：
- `TYPELAYOUT_REGISTER_OPAQUE` — 要求 `trivially_copyable`
- `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE` — 无此要求
- `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` — 单参数容器模板
- `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` — 双参数映射模板

> "implementation-defined fields"

**准确。** 代码中的完整处理链：
- `long`：type_map.hpp:42-55 检测是否与 `int32_t`/`int64_t` 相同，不同则按实际 `sizeof` 映射
- `wchar_t`：type_map.hpp:117 使用实际 `sizeof`/`alignof` 编码
- `long double`：config.hpp:79-95 通过 `__LDBL_MANT_DIG__` 区分四种表示
- safety_level.hpp:55-61 将以上类型标记为 `PlatformVariant` 级别

---

## 第 61–64 行 — 审稿人说明

> "This is not a general reflection overview. It is a concrete C++26 reflection application aimed at a real systems problem"

**准确。** 提案聚焦于一个具体的工程问题，而非 P2996 的功能介绍。

> "The talk includes real code, generated artifacts, and cross-target verification examples"

**准确（作为演讲规划）。** 库具备所有描述的能力：代码（核心层 + 工具层）、生成产物（`.sig.hpp`）、跨目标验证（`CompatReporter`）。

> "The core workflow is backed by reflection-capable implementations available today"

**准确。** GCC trunk 于 2026-01-15 合并 P2996 支持。Bloomberg Clang P2996 分支为实验性但可用。项目在两者上均可构建。

> "cross-platform artifacts are pre-generated where a second target is not part of the live path"

**准确且诚实。** 坦承了现场演示中的工具链约束。

> "The talk is framed around the method and engineering workflow, not around claiming uniform toolchain coverage"

**准确。** 恰当地将重点放在方法与工作流上，而非工具链覆盖率。

---

## 审计总结

| 行号 | 声明摘要 | 结论 |
|------|----------|------|
| 5 | 标题：编译期 memcpy 安全验证 | 准确 |
| 13 | `trivially_copyable` 不足以保证传输安全 | 已验证（admission.hpp） |
| 13 | 同一源码在不同平台可编译为不同表示 | 已验证（type_map.hpp + config.hpp） |
| 15 | 反射可枚举字段/基类/偏移/位域 | 已验证（reflect.hpp + signature_impl.hpp） |
| 15 | 端到端 CI 工作流 | 已验证（sig_export.hpp + compat_check.hpp） |
| 17 | 不证明语义兼容性 | 已验证（CLAUDE.md 设计取舍） |
| 22 | 无 IDL/序列化桩/运行时开销 | 已验证：全部 consteval |
| 37 | 示例签名 `[64-le]record[s:16,a:8]{...}` | 已通过代码追踪验证 |
| 38 | 位域/数组/空基类边界情况 | 已验证（signature_impl.hpp + type_map.hpp） |
| **51** | **"pinpoints which types and which fields differ"** | **[已修复] 新增 `format_field_diff`，字段级差异输出现已兑现** |
| 57 | 虚继承/不透明类型/实现定义字段处理 | 已验证（跨多个源文件） |
| 64 | 由现有实现支撑 | 已验证（GCC 16 trunk + Bloomberg Clang） |

---

## 需要修正的问题

**原发现 1 处事实性问题；已通过代码改动消除，无需修改提案文本。**

### 第 51 行 — "which fields differ"（已修复）

原审计建议修改提案措辞以匹配 `CompatReporter` 的实际输出。最终采取相反路径：**通过扩展代码让实现对齐提案原意**。

新增实现：
- `detail::parse_sig_fields()` — 括号深度感知地切分 `{...}` 成员列表
- `CompatReporter::format_field_diff()` — 按位置比对字段，只列出差异项并标注 header 差异

已通过独立构造的三平台 `PlatformInfo` 数据验证字段级差异输出正确、现有测试全部通过。提案第 51 行保持原文不变。

---

## 其余声明

提案中所有其他事实性声明均与代码库一致，无需修改。提案在抽象层次上恰当地概括了库的能力，未发现夸大或遗漏关键限制的情况。
