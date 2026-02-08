# Completeness Analysis: PROOFS.md vs Code & Design Intent

## 方法论

从三个维度交叉验证完备性：
1. **核心价值 (V1/V2/V3)** → 每个承诺是否有对应的形式化定理？
2. **代码路径** → 每条实现路径是否被证明覆盖？
3. **已知限制** → 证明假设是否完整覆盖所有边界条件？

---

## 1. 核心价值 × 证明定理 矩阵

### V1: 布局签名的可靠性
**承诺**: `layout_sig(T) == layout_sig(U) ⟹ memcmp-compatible(T, U)`

| 需要证明的性质 | PROOFS.md 定理 | 状态 | 说明 |
|---|---|---|---|
| ⟦·⟧_L 是 L_P 的忠实编码 | Theorem 3.1 (Encoding Faithfulness) | ✅ 完备 | decode 存在且正确（已修为部分函数） |
| ⟦·⟧_L 签名相同 → 布局相同 | Theorem 3.2 (Soundness) | ✅ 完备 | 由 3.1 逆否命题直接推出 |
| 布局相同 → 签名相同（单射性） | Corollary 3.1.1 (Injectivity) | ✅ 完备 | L_P(T) ≠ L_P(U) ⟹ ⟦T⟧_L ≠ ⟦U⟧_L |
| 签名不同不一定布局不兼容（保守性） | Theorem 3.3 (Conservativeness) | ✅ 完备 | 用 ≅_byte vs ≅_mem 修复了逻辑矛盾 |
| 偏移量正确性 | Theorem 3.4 (Offset Correctness) | ✅ 完备 | 结构归纳法 |

**V1 评估: ✅ 完备**。所有 V1 承诺都有对应的形式化定理，且逻辑链完整。

### V2: 结构签名的精确性
**承诺**: `def_sig(T) == def_sig(U) ⟹ T 和 U 的字段名、类型、层次完全一致`

| 需要证明的性质 | PROOFS.md 定理 | 状态 | 说明 |
|---|---|---|---|
| ⟦·⟧_D 是 D_P 的忠实编码 | *(隐含于 §4)* | ⚠️ **缺失** | Theorem 3.1 只证了 ⟦·⟧_L 的忠实性，⟦·⟧_D 的忠实性在 Theorem 4.2 中被引用但未独立证明 |
| ⟦·⟧_D 签名相同 → 结构相同 | *(隐含于 V3 证明)* | ⚠️ **缺失** | 没有独立的 "Definition Soundness" 定理 |
| ⟦·⟧_D 保留字段名 | Definition 2.2 | ✅ 定义覆盖 | 但无独立定理证明字段名的忠实保留 |
| ⟦·⟧_D 保留继承结构 | Definition 2.2 | ✅ 定义覆盖 | ~base<> / ~vbase<> 在定义中明确 |
| ⟦·⟧_D 保留枚举限定名 | Definition 2.2 Case 6 | ✅ 定义覆盖 | qualified_name(E) 明确包含 |

**V2 评估: ⚠️ 部分缺失**。V2 的核心保证（Definition 签名的忠实编码性）没有独立的形式化定理。
Theorem 4.2 的证明中引用了 "⟦·⟧_D is also a faithful encoding"，但这只是一句断言，
没有对应 ⟦·⟧_D 的独立 Encoding Faithfulness 定理。

### V3: 两层的投影关系
**承诺**: `def_match(T, U) ⟹ layout_match(T, U)`（反之不成立）

| 需要证明的性质 | PROOFS.md 定理 | 状态 | 说明 |
|---|---|---|---|
| Definition 匹配 → Layout 匹配 | Theorem 4.2 (V3 Projection) | ✅ 完备 | 语义论证 via R_P |
| Layout 匹配 ↛ Definition 匹配 | Theorem 4.3 (Strict Refinement) | ✅ 完备 | ker(⟦·⟧_D) ⊊ ker(⟦·⟧_L) |

**V3 评估: ✅ 完备**。

---

## 2. 代码路径 × 证明覆盖 矩阵

### signature_detail.hpp Part 4: TypeSignature 特化

| 代码路径 (特化) | 行号 | σ 定义 (Def 1.2) | Denotation 定义 (Def 2.1/2.2) | 归纳验证 (§5) |
|---|---|---|---|---|
| int8_t / uint8_t / ... / uint64_t | 315-322 | ✅ | ✅ Case 1 | ✅ §5.1 |
| signed char (when ≠ int8_t) | 326-329 | ✅ | ✅ Case 1 | ⚠️ 未显式列举 |
| unsigned char (when ≠ uint8_t) | 332-335 | ✅ | ✅ Case 1 | ⚠️ 未显式列举 |
| long (conditional) | 338-344 | ✅ | ✅ Case 1 | ✅ §5.1 表格 |
| unsigned long (conditional) | 346-353 | ✅ | ✅ Case 1 | ⚠️ 仅"analogous" |
| long long (when ≠ int64_t) | 355-359 | ✅ | ✅ Case 1 | ⚠️ 未显式列举 |
| unsigned long long (when ≠ uint64_t) | 361-365 | ✅ | ✅ Case 1 | ⚠️ 未显式列举 |
| float / double | 368-369 | ✅ | ✅ Case 1 | ✅ §5.1 表格 |
| long double | 370-372 | ✅ | ✅ Case 1 | ✅ §5.1 表格 |
| char / wchar_t / char8_t / char16_t / char32_t | 375-379 | ✅ | ✅ Case 1 | ✅ §5.1 表格 (char, wchar_t) |
| bool | 382 | ✅ | ✅ Case 1 | ⚠️ 未在表格中 |
| std::nullptr_t | 383 | ✅ | ✅ Case 1 | ⚠️ 未在表格中 |
| std::byte | 384 | ✅ | ✅ Case 1 | ⚠️ 未在表格中 |
| R(*)(Args...) (+ noexcept, variadic) | 387-413 | ✅ | ✅ Case 1 | ⚠️ 未在表格中 |
| const T / volatile T / const volatile T | 416-427 | ✅ Def 1.6 | ✅ Case 8 | ⚠️ 无独立归纳项 |
| T* / T& / T&& / T C::* | 430-445 | ✅ | ✅ Case 1 | ✅ §5.1 表格 (ptr, memptr) |
| T[] (unbounded) | 448-454 | N/A (static_assert) | N/A | N/A (编译错误) |
| T[N] (bounded array) | 463-475 | ✅ | ✅ Case 4 | ✅ §5.5 |
| enum | 481-498 | ✅ | ✅ Case 6 | ✅ §5.8 |
| union | 499-508 | ✅ | ✅ Case 5 | ✅ §5.6 |
| class (Layout) | 510-530 | ✅ | ✅ Case 2/3 | ✅ §5.2-5.4 |
| class (Definition) | 531-543 | ✅ | ✅ Def 2.2 Case 2/3 | ✅ §5.2-5.4 |
| void | 545-548 | N/A (static_assert) | N/A | N/A |
| function type | 549-552 | N/A (static_assert) | N/A | N/A |
| else (fallback) | 553-556 | N/A (static_assert) | N/A | N/A |

### signature_detail.hpp Part 2/3: 签名引擎

| 引擎逻辑 | 代码位置 | 证明覆盖 |
|---|---|---|
| `definition_field_signature` (bit-field 分支) | 85-96 | ✅ Def 1.3 + Def 2.2 Case 7 |
| `definition_field_signature` (normal 分支) | 98-103 | ✅ Def 2.2 fields_part_D |
| `definition_base_signature` (virtual 分支) | 136-137 | ✅ Def 2.2 bases_part_D |
| `definition_base_signature` (non-virtual 分支) | 139-141 | ✅ Def 2.2 bases_part_D |
| `definition_content` (bases + fields 合并) | 169-176 | ✅ Def 2.2 |
| `layout_field_with_comma` (bit-field 分支) | 194-204 | ✅ Def 1.3 bit-field |
| `layout_field_with_comma` (class, non-union → 递归) | 205-207 | ✅ Def 1.3 flatten recursive |
| `layout_field_with_comma` (else → leaf) | 208-213 | ✅ Def 1.3 else branch |
| `layout_one_base_prefixed` (base offset) | 222-228 | ✅ Theorem 3.4 inheritance case |
| `layout_all_prefixed` (bases + fields 合并) | 236-245 | ✅ Def 1.3 |
| `get_layout_content` → `skip_first()` | 247-250 | ⚠️ **未证明** skip_first 不丢失信息 |
| `layout_union_field` (不展平) | 254-277 | ✅ Def 2.1 Case 5 |
| `get_layout_union_content` | 292-299 | ✅ |

### signature.hpp: 公共 API

| API | 代码位置 | 证明覆盖 |
|---|---|---|
| `get_arch_prefix()` | 12-19 | ✅ Def 1.1 + Def 2.1 full_sig_L |
| `get_layout_signature<T>()` | 24-26 | ✅ = arch prefix + ⟦T⟧_L |
| `layout_signatures_match<T1,T2>()` | 29-31 | ✅ = (⟦T1⟧_L == ⟦T2⟧_L) |
| `get_definition_signature<T>()` | 36-38 | ✅ = arch prefix + ⟦T⟧_D |
| `definition_signatures_match<T1,T2>()` | 40-43 | ✅ |

### tools/ 层

| 工具功能 | 代码位置 | 证明覆盖 | 说明 |
|---|---|---|---|
| `sig_match` (string comparison) | compat_check.hpp:36 | ✅ | 字符串相等 = 签名相等 |
| `classify_safety` | compat_check.hpp:67-91 | ⚠️ **未涉及** | 安全分类逻辑不在证明范围内 |
| `CompatReporter::compare` | compat_check.hpp:180-228 | ⚠️ **未涉及** | 运行时比较逻辑不在证明范围内 |
| `SigExporter::add<T>` | sig_export.hpp:60-69 | ✅ | 调用 get_layout/definition_signature |
| `all_layouts_match` | compat_auto.hpp:111-126 | ✅ | 逐字符比较 = 字符串相等 |

**代码路径评估**: 核心签名生成路径全部覆盖。tools 层为应用层，不在形式化证明范围内（合理）。

---

## 3. Gap Analysis — 需要但未证明

### Gap 1 (Medium): ⟦·⟧_D 的忠实编码性缺乏独立证明

**现状**: Theorem 4.2 的证明中声称 "⟦·⟧_D is also a faithful encoding (by the Definition grammar's
unambiguity, extending Lemma 1.8.1)"，但这只是一句引理级断言，没有独立的定理。

**影响**: V2 核心承诺 "def_sig(T) == def_sig(U) ⟹ 结构完全一致" 缺乏直接的形式化支撑。
虽然 Definition denotation 的定义 (Def 2.2) 已经清晰，但从定义到"忠实编码"需要一个显式的
证明步骤——至少需要证明 Definition 语法也是无歧义的。

**建议**: 补充 **Theorem 3.1' (Definition Encoding Faithfulness)** 和 **Corollary (Definition Soundness)**。

### Gap 2 (Low): §5.1 表格不完整

**现状**: §5.1 的验证表格只列了 9 种类型 (int32_t, float, double, char, long, wchar_t, long double, T*, T C::*)。
但 σ 定义覆盖了 25+ 种类型（包括 bool, byte, nullptr, ref, rref, fnptr, char8_t, char16_t, char32_t 等）。

**影响**: 不影响证明逻辑正确性（表格是辅助说明），但影响完备性的直觉感受。

**建议**: 扩展表格或添加说明 "其他类型通过相同的 kind + format_size_align 模式正确"。

### Gap 3 (Low): skip_first() 的正确性未形式化

**现状**: Layout 引擎使用 comma-prefixed 策略累积字段，最终通过 `skip_first()` 去掉前导逗号。
这个变换的正确性（不丢失信息）未在证明中提及。

**影响**: 极低。`skip_first()` 是纯字符串操作，仅去除第一个字符。但严格来说，
证明应确认 "去除前导逗号不影响后续 decode 的可逆性"。

**建议**: 在 Def 2.1 或 Theorem 3.1 附近添加一句备注即可。

### Gap 4 (Medium): Definition 语法 (Grammar for ⟦·⟧_D) 未显式给出

**现状**: §1.8 只给出了 Layout 签名的语法。Definition 签名的语法（包含 `[name]`、`~base<>`、
`,polymorphic`、`enum<qualified_name>` 等额外产生式）未显式定义。

**影响**: 这使得 Theorem 4.2 中引用的 "Definition grammar's unambiguity" 缺乏根基。

**建议**: 在 §1.8 之后补充 §1.9 "Definition Signature Grammar"，或在 §1.8 中扩展。

---

## 4. Consistency Check — 描述不一致

### Issue 1: §6 图表中的 "4.1 π def" 过时

**现状**: §6 的 ASCII 架构图中写着 `4.1 π def`，但 §4.1 已重命名为 "Definition 4.1 (Reflection Data)"，
且 π (erasure function) 已在 Theorem 4.2 的修复中明确放弃。

**建议**: 更新为 `4.1 R_P def`。

### Issue 2: Property 1.2.1 引用 "Lemma 1.7"（不存在）

**现状**: Property 1.2.1 的证明中引用 "By Lemma 1.7 (grammar unambiguity)"，
但实际编号是 "Lemma 1.8.1"。§1.7 是 "Definition 1.7 (memcmp-compatibility)"。

**建议**: 修正引用为 "Lemma 1.8.1"。

---

## 5. 完备性总结

| 维度 | 状态 | 详情 |
|---|---|---|
| V1 (Layout 可靠性) | ✅ **完备** | 5 个定理完整覆盖 |
| V2 (Definition 精确性) | ⚠️ **部分缺失** | ⟦·⟧_D 忠实编码性缺独立定理 + Definition 语法未显式给出 |
| V3 (投影关系) | ✅ **完备** | Theorem 4.2 + 4.3 覆盖 |
| 代码路径覆盖 | ✅ **完备** | 所有 TypeSignature 特化均有对应定义/定理 |
| 假设完整性 | ✅ **完备** | §6.5 覆盖平台、类型系统、语义假设 |
| 内部一致性 | ⚠️ **2 处不一致** | 图表标签过时 + 引理引用错误 |

**总体判定**: 证明体系 **基本完备**，V1 和 V3 完全覆盖，V2 需要补充 2 个形式化内容
（Definition Faithfulness 定理 + Definition Grammar），以及修正 2 处文字不一致。
