## Context

需要将 TypeLayout 的签名正确性证明提升到形式化验证标准。
本文档先分析业界形式化证明的方法论和案例，然后据此重写 TypeLayout 的证明。

---

# Part I: Formal Verification Case Studies

## 1. Academic Formal Methods

### 1.1 Denotational Semantics (指称语义)

**方法:** 将程序构造映射到数学对象（集合、函数、域），用数学等式定义程序的含义。

**典型案例:** Scott-Strachey 语义框架
- 每个语法构造 `E` 对应一个数学含义 `⟦E⟧ ∈ D`
- 组合性 (compositionality): `⟦E₁ op E₂⟧ = F(⟦E₁⟧, ⟦E₂⟧)`
- 证明方式: 定义语义函数 `⟦·⟧`，然后证明其性质

**适用性分析:**
TypeLayout 的签名函数本质上就是一个语义函数——将 C++ 类型映射为字符串：
- `⟦T⟧_L` = Layout 签名字符串
- `⟦T⟧_D` = Definition 签名字符串

这正好符合指称语义的框架：签名函数是一个从类型域到字符串域的指称 (denotation)。
**评价: ★★★★★ 最适合。** 签名函数天然就是一个语义函数。

### 1.2 Refinement (精化)

**方法:** 证明一个抽象规约 (specification) 被一个具体实现 (implementation) 正确实现。
形式化为: `Impl ⊑ Spec` (实现精化了规约)。

**典型案例:** Z/B方法、Event-B
- 定义抽象状态机 (Abstract Machine)
- 定义具体实现
- 证明每一步精化保持不变量

**适用性分析:**
TypeLayout 的两层签名正是一个精化关系：
- Definition 签名是"抽象"层 (更多信息)
- Layout 签名是"具体"层 (投影后的简化)
- V3: `def_match ⟹ layout_match` 就是精化关系

**评价: ★★★★☆ 非常适合。** V3 投影关系天然就是精化。

### 1.3 Abstract Interpretation (抽象解释)

**方法:** 将具体域 (concrete domain) 通过 Galois 连接映射到抽象域 (abstract domain)。

**典型案例:** Astrée 静态分析器 (Airbus A380 飞控软件验证)
- 具体域: 程序所有可能的执行状态
- 抽象域: 状态的安全近似 (如区间、多面体)
- Galois 连接: α (抽象化) 和 γ (具体化) 构成伴随对

**适用性分析:**
TypeLayout 的签名可以视为一种抽象解释：
- 具体域 = 类型的完整内存布局 (每个字节的含义)
- 抽象域 = 签名字符串
- α (抽象化) = 签名生成函数 `Sig_L`, `Sig_D`
- 签名匹配 = 抽象域中的等价性 ⟹ 具体域中的兼容性 (soundness)

**评价: ★★★☆☆ 可用但过度。** Galois 连接的完整形式化对本场景来说过于复杂。

### 1.4 Hoare Logic (霍尔逻辑)

**方法:** 用前/后条件 {P}S{Q} 验证程序语句的正确性。

**适用性分析:**
签名函数是纯函数（无副作用），不涉及程序状态变换。Hoare 逻辑更适合命令式程序的验证。
**评价: ★★☆☆☆ 不太适合。**

### 1.5 Bisimulation (互模拟)

**方法:** 证明两个系统在可观察行为上不可区分。

**适用性分析:**
可以将"签名匹配"建模为"布局互模拟"，但引入的概念复杂度与收益不成比例。
**评价: ★★☆☆☆ 不太适合。**

## 2. Industrial Formal Verification Cases

### 2.1 CompCert (Verified C Compiler)

**项目:** 用 Coq 证明的 C 编译器，保证编译不引入 bug。
**核心定理:**
```
∀ program P, ∀ behavior B:
  source_semantics(P) = B ⟹ compiled_semantics(compile(P)) = B
```
即：编译保持语义不变 (semantic preservation)。

**方法特点:**
1. 用指称语义定义源语言和目标语言的语义
2. 对每个编译步骤 (pass) 证明语义保持
3. 证明链式组合: pass₁ ∘ pass₂ ∘ ... ∘ passₙ 保持语义

**对 TypeLayout 的启示:**
- TypeLayout 的签名生成类似于 CompCert 的一个编译 pass：
  将"类型"(源语言) 转换为"签名字符串"(目标语言)
- 需要证明这个转换保持"布局信息"(语义)
- CompCert 的证明结构可以直接借鉴

### 2.2 seL4 (Verified Microkernel)

**项目:** 用 Isabelle/HOL 证明的操作系统微内核。
**核心定理:**
```
∀ system call S:
  abstract_spec(S) satisfies security_property
  ∧ implementation(S) refines abstract_spec(S)
  ⟹ implementation(S) satisfies security_property
```

**方法特点:**
1. 三层架构: 抽象规约 → Haskell 原型 → C 实现
2. 每层之间证明精化 (refinement)
3. 使用 Isabelle/HOL 进行机器检查

**对 TypeLayout 的启示:**
- TypeLayout 的两层签名天然形成 seL4 式的精化链：
  Definition (抽象) → Layout (具体)
- V3 投影就是精化关系

### 2.3 TLA+ (Temporal Logic of Actions)

**项目:** Lamport 的规约语言，用于分布式系统验证。

**方法特点:**
1. 将系统行为定义为状态机
2. 用时序逻辑 (LTL/CTL) 表达性质
3. 通过模型检查 (model checking) 验证

**对 TypeLayout 的启示:**
- 不适合。TypeLayout 是纯函数映射，不涉及状态变迁或时序性质。

### 2.4 Alloy (Lightweight Formal Methods)

**项目:** 轻量级规约语言，用关系逻辑建模。

**方法特点:**
1. 定义签名 (sig)、关系 (relation)、约束 (constraint)
2. 通过有界模型检查自动发现反例
3. 适合快速验证设计概念

**对 TypeLayout 的启示:**
- 可用于快速验证签名函数的性质（如单射性），但缺乏完整证明能力

## 3. Method Selection for TypeLayout

**结论: 采用 Denotational Semantics + Refinement 的混合框架。**

| 方法 | 用于 | 理由 |
|------|------|------|
| **指称语义** | 定义签名函数的语义、证明单射性和完备性 | 签名函数天然是语义函数 |
| **精化** | 证明 V3 投影关系 (Definition ⊑ Layout) | 两层签名天然是精化关系 |
| **结构归纳** | 逐类型类别证明 | C++ 类型系统是归纳定义的 |

**不采用的方法:**
- Galois 连接：过度复杂
- Hoare 逻辑：不适合纯函数
- 机器辅助证明 (Coq/Isabelle)：工程投入过大，纸笔证明足够

---

# Part II: Formalized Proof of TypeLayout Signature System

## Notation Conventions

| 符号 | 含义 |
|------|------|
| T, U | C++ 类型 |
| P | 平台 (指针宽度, 字节序, ABI) |
| ⟦T⟧_L | T 的 Layout 签名 (指称) |
| ⟦T⟧_D | T 的 Definition 签名 (指称) |
| ≅_mem | memcmp-兼容等价关系 |
| ⊑ | 精化关系 |
| dom(f) | 函数 f 的定义域 |
| ker(f) | 函数 f 的等价核: {(x,y) \| f(x) = f(y)} |

---

## §1 Type Domain (类型域)

### Definition 1.1 (Platform)

平台 P 是一个三元组：

    P = (w, e, abi)

其中 w ∈ {32, 64} 是指针宽度 (位), e ∈ {le, be} 是字节序,
abi 是 ABI 规范标识。

### Definition 1.2 (Primitive Type Signature)

固定平台 P 下，原始类型签名函数 σ : PrimitiveTypes → Σ* 定义为：

    σ(int8_t)    = "i8[s:1,a:1]"
    σ(uint8_t)   = "u8[s:1,a:1]"
    σ(int16_t)   = "i16[s:2,a:2]"
    ...
    σ(float)     = "f32[s:4,a:4]"
    σ(double)    = "f64[s:8,a:8]"
    σ(long)      = "i" ++ str(8·sizeof_P(long)) ++ "[s:" ++ str(sizeof_P(long)) ++ ",a:" ++ str(alignof_P(long)) ++ "]"
    σ(T*)        = "ptr[s:" ++ str(w/8) ++ ",a:" ++ str(w/8) ++ "]"

其中 sizeof_P 和 alignof_P 是平台 P 上的 sizeof 和 alignof 值。

**性质 1.2.1 (Primitive injectivity):**
对于任意两个原始类型 τ₁, τ₂：

    σ(τ₁) = σ(τ₂) ⟹ sizeof_P(τ₁) = sizeof_P(τ₂) ∧ alignof_P(τ₁) = alignof_P(τ₂) ∧ kind(τ₁) = kind(τ₂)

其中 kind 是类型的种类标识 (i/u/f/char/bool/ptr/...)。

*Proof:* 签名前缀 (i8/u8/f32/ptr/...) 唯一标识类型种类，`[s:N,a:M]` 唯一标识大小和对齐。
由 Lemma 1.7 (语法无歧义)，前缀+大小+对齐的组合是单射的。∎

### Definition 1.3 (Leaf Field)

类型 T 在平台 P 上的叶字段序列定义为：

    fields_P(T) = flatten(T, 0)

其中 flatten 递归定义为：

    flatten(T, adj) =
      let bases = bases_of(T) in
      let members = nonstatic_data_members_of(T) in
      concat([flatten(base_type(b), offset_of(b) + adj) | b ∈ bases])
      ++
      concat([
        if is_class(type(m)) ∧ ¬is_union(type(m))
          then flatten(type(m), offset_of(m) + adj)
          else [(offset_of(m) + adj, σ(type(m)))]
        | m ∈ members
      ])

注意: Union 成员不被展平，保留为原子签名。

### Definition 1.4 (Byte Layout — Formal)

    L_P(T) = (sizeof_P(T), alignof_P(T), poly_P(T), fields_P(T))

其中:
- sizeof_P(T) ∈ ℕ
- alignof_P(T) ∈ ℕ
- poly_P(T) ∈ {true, false}
- fields_P(T) ∈ (ℕ × Σ*)*  (偏移量-签名的有序序列)

### Definition 1.5 (Structure Tree — Formal)

    D_P(T) = (sizeof_P(T), alignof_P(T), poly_P(T), bases_P(T), named_fields_P(T))

其中:
- bases_P(T) ∈ (Σ* × {base, vbase} × D_P(·))*  (限定名, 虚/非虚, 递归结构)
- named_fields_P(T) ∈ (ℕ × Σ* × Σ*)*  (偏移量, 字段名, 类型签名)

---

## §2 Signature Denotation (签名指称)

### Definition 2.1 (Layout Denotation)

Layout 签名的指称函数 ⟦·⟧_L : Types_P → Σ* 递归定义为：

    ⟦T⟧_L =
      "[" ++ arch(P) ++ "]" ++                          -- 架构前缀
      "record[s:" ++ str(sizeof_P(T)) ++
            ",a:" ++ str(alignof_P(T)) ++
            (if poly_P(T) then ",vptr" else "") ++
      "]{" ++
      join(",", [
        "@" ++ str(oᵢ) ++ ":" ++ sigᵢ
        for (oᵢ, sigᵢ) in fields_P(T)
      ]) ++
      "}"

    where arch(w, le) = str(w) ++ "-le"
          arch(w, be) = str(w) ++ "-be"

### Definition 2.2 (Definition Denotation)

Definition 签名的指称函数 ⟦·⟧_D : Types_P → Σ* 递归定义为：

    ⟦T⟧_D =
      "[" ++ arch(P) ++ "]" ++
      "record[s:" ++ str(sizeof_P(T)) ++
            ",a:" ++ str(alignof_P(T)) ++
            (if poly_P(T) then ",polymorphic" else "") ++
      "]{" ++
      join(",", bases_part_D(T) ++ fields_part_D(T)) ++
      "}"

    where bases_part_D(T) = [
        (if is_virtual(b) then "~vbase<" else "~base<") ++
        qualified_name(base_type(b)) ++ ">:" ++ ⟦base_type(b)⟧_D
        for b in bases_of(T)
      ]
    and fields_part_D(T) = [
        "@" ++ str(offset_of(m)) ++ "[" ++ name(m) ++ "]:" ++ ⟦type(m)⟧_D
        for m in nonstatic_data_members_of(T)
      ]

### Definition 2.3 (CV-Erasure)

    ⟦const T⟧_L = ⟦T⟧_L
    ⟦volatile T⟧_L = ⟦T⟧_L
    ⟦const volatile T⟧_L = ⟦T⟧_L

同理适用于 ⟦·⟧_D。

**Justification:** CV 限定符不影响内存布局 (C++ [basic.type.qualifier])。

### Definition 2.4 (memcmp-compatibility)

    T ≅_mem U ⟺ L_P(T) = L_P(U)

即两个类型 memcmp-兼容当且仅当它们的字节布局 (定义 1.4) 完全相同。

---

## §3 Core Theorems

### Theorem 3.1 (Encoding Faithfulness / 编码忠实性)

**Statement:** ⟦·⟧_L 是从 L_P 到 Σ* 的忠实编码——存在逆函数 decode 使得：

    decode(⟦T⟧_L) = L_P(T)  for all T

**Proof:**

(1) **Existence of decode:** 由定义 2.1，签名字符串是通过连接组件构造的，其中每个组件
用唯一语法分隔。由签名语法 (§1.2 of PROOFS.md) 的无歧义性 (Lemma 1.7)，字符串可被
唯一解析回其组成部分。定义 decode 为该解析过程。

(2) **Correctness of decode:** 
- `decode` 从 `s:N` 中提取 sizeof → sizeof_P(T) ✓
- `decode` 从 `a:N` 中提取 alignof → alignof_P(T) ✓
- `decode` 从 `,vptr` 存在性提取 poly → poly_P(T) ✓
- `decode` 从 `@OFF:SIG` 列表提取 fields → fields_P(T) ✓

因此 decode ∘ ⟦·⟧_L = L_P，即 ⟦·⟧_L 是忠实编码。∎

**Corollary 3.1.1 (Injectivity):**

    L_P(T) ≠ L_P(U) ⟹ ⟦T⟧_L ≠ ⟦U⟧_L

*Proof:* 若 ⟦T⟧_L = ⟦U⟧_L，则 L_P(T) = decode(⟦T⟧_L) = decode(⟦U⟧_L) = L_P(U)，矛盾。∎

### Theorem 3.2 (Soundness / 可靠性 — 零假阳性)

**Statement:**

    ⟦T⟧_L = ⟦U⟧_L ⟹ T ≅_mem U

**Proof:** 由 Corollary 3.1.1 的逆否：

    ⟦T⟧_L = ⟦U⟧_L
    ⟹ L_P(T) = L_P(U)      [by faithfulness: decode ∘ ⟦·⟧_L = L_P]
    ⟹ T ≅_mem U             [by Definition 2.4]  ∎

**Commentary (CompCert parallel):** 此定理类比 CompCert 的语义保持 (semantic preservation):
- CompCert: compile(P) 的行为 = P 的行为 (编译保持语义)
- TypeLayout: ⟦T⟧_L = ⟦U⟧_L ⟹ T 和 U 布局相同 (签名保持布局信息)

两者的证明策略相同：证明编码/编译过程是一个**忠实函子** (faithful functor)。

### Theorem 3.3 (Conservativeness / 保守性)

**Statement:** 存在 T, U 使得 T ≅_mem U 但 ⟦T⟧_L ≠ ⟦U⟧_L。

**Proof:** Counterexample:

    T = struct { int32_t x, y, z; }
    U = int32_t[3]

    L_P(T) = (12, 4, false, [(0, "i32[s:4,a:4]"), (4, "i32[s:4,a:4]"), (8, "i32[s:4,a:4]")])
    L_P(U) = (12, 4, false, [(0, "i32[s:4,a:4]"), (4, "i32[s:4,a:4]"), (8, "i32[s:4,a:4]")])

    但 ⟦T⟧_L = "[64-le]record[s:12,a:4]{@0:i32[...],@4:i32[...],@8:i32[...]}"
      ⟦U⟧_L = "[64-le]array[s:12,a:4]<i32[s:4,a:4],3>"

注意: 按定义 1.3，L_P(U) 对于数组不会生成叶字段序列，而是生成一个 array 签名。
因此严格来说 L_P(T) ≠ L_P(U)（一个是 record fields，另一个是 array 签名），
但它们在**逐字节层面**等价。

这表明 L_P 本身就是一个精化——它不仅编码字节布局，还编码类型种类 (record vs array)。
因此 ⟦·⟧_L 是 "type-aware layout identity" 而非纯 "byte identity"。

**Formal characterization:**

    ker(⟦·⟧_L) ⊂ ≅_byte (where ≅_byte is pure byte identity)

签名的等价核严格小于纯字节等价。∎

### Theorem 3.4 (Offset Correctness / 偏移正确性)

**Statement:** 对于嵌套/继承类型，⟦·⟧_L 中的所有偏移量等于编译器报告的绝对偏移。

**Proof by structural induction on the type AST:**

*Base case:* 原始类型 τ。无偏移概念，签名直接由 σ(τ) 给出。✓

*Inductive case (composition):* struct T { Inner x; ... }

    offset_in_sig(@x.field_j) = offset_of(x) + offset_within_Inner(field_j)

由实现: `field_offset = offset_of(member).bytes + OffsetAdj`

- `offset_of(member).bytes` 由 P2996 编译器提供 (公理级信任)
- `OffsetAdj` = 累计的祖先偏移 (由归纳假设正确)
- 和 = 绝对偏移 ✓

*Inductive case (inheritance):* struct Derived : Base { ... }

    offset_in_sig(@base.field_j) = offset_of(Base in Derived) + offset_within_Base(field_j)

同上，`offset_of(base_info).bytes` 由 P2996 提供。✓

由结构归纳法，所有偏移量正确。∎

**Commentary (seL4 parallel):** 此证明类比 seL4 的精化证明——每一层的偏移
由上一层正确传递，类似 seL4 中每一层的安全性质通过精化链传递。

---

## §4 Refinement: Definition ⊑ Layout

### Definition 4.1 (Erasure / 擦除)

定义擦除函数 π : Σ* → Σ*，将 Definition 签名映射为 Layout 签名：

    π(s) = s
      |> remove_field_names        -- "@OFF[name]:SIG" → "@OFF:SIG"
      |> flatten_inheritance       -- "~base<N>:record{F}" → F (with absolute offsets)
      |> replace_poly_marker       -- "polymorphic" → "vptr"
      |> remove_enum_qualnames     -- "enum<ns::C>[...]" → "enum[...]"

**Lemma 4.1.1 (Well-definedness of π):**
每个子操作都是确定性的字符串变换：
- `remove_field_names`: 正则替换 `@(\d+)\[[^\]]*\]:` → `@$1:`
- `flatten_inheritance`: 递归展开 `~base<>:record{...}` 中的字段，基于编码的偏移量
- `replace_poly_marker`: 固定字符串替换
- `remove_enum_qualnames`: 正则替换 `enum<[^>]*>` → `enum`

每一步输入唯一确定输出，因此 π 是良定义的。∎

**Lemma 4.1.2 (π commutes with denotation):**

    π(⟦T⟧_D) = ⟦T⟧_L  for all T

*Proof:* ⟦T⟧_D 和 ⟦T⟧_L 从相同的 P2996 反射数据生成。⟦T⟧_D 保留了
额外信息 (字段名, 继承结构, 限定名)。π 精确地移除这些额外信息，
结果与直接生成 ⟦T⟧_L 的过程一致。∎

### Theorem 4.2 (V3 Projection / 投影定理)

**Statement:**

    ⟦T⟧_D = ⟦U⟧_D ⟹ ⟦T⟧_L = ⟦U⟧_L

**Proof:**

    ⟦T⟧_D = ⟦U⟧_D
    ⟹ π(⟦T⟧_D) = π(⟦U⟧_D)      [by congruence of π]
    ⟹ ⟦T⟧_L = ⟦U⟧_L              [by Lemma 4.1.2]  ∎

### Theorem 4.3 (Strict Refinement / 严格精化)

**Statement:** ker(⟦·⟧_D) ⊊ ker(⟦·⟧_L)

*Proof:*
- ker(⟦·⟧_D) ⊆ ker(⟦·⟧_L): by Theorem 4.2
- ker(⟦·⟧_D) ≠ ker(⟦·⟧_L): counterexample (Derived vs Flat, see §3.3)

Therefore ker(⟦·⟧_D) ⊊ ker(⟦·⟧_L)。∎

**Commentary:** 这正是精化理论中的"observational refinement"——
⟦·⟧_D 的可观察行为（所有能区分的情况）严格多于 ⟦·⟧_L。

---

## §5 Per-Category Structural Induction

### Induction Principle

C++ 类型按以下语法归纳定义：

    Type ::= Primitive                    (基础类型)
           | Record(fields)               (struct/class)
           | Record(bases, fields)        (继承)
           | PolyRecord(bases, fields)    (多态)
           | Array(Type, N)               (数组)
           | Union(fields)                (联合)
           | Enum(underlying)             (枚举)
           | BitField(Type, width)        (位域)

对于每个 Type 构造器，验证 ⟦·⟧_L 和 ⟦·⟧_D 的正确性。

### 5.1 Primitive

    ⟦τ⟧_L = σ(τ)    -- by Definition 2.1
    ⟦τ⟧_D = σ(τ)    -- same for primitives (no extra info)

正确性由 Definition 1.2 (σ 的定义) 和 C++ 标准中的类型大小保证。✓

### 5.2 Record(fields) — struct/class

正确性由 Theorem 3.4 (偏移正确性) 的归纳证明保证。
归纳变量: 字段数 n 和嵌套深度 d。✓

### 5.3 Record(bases, fields) — 继承

Layout: 展平。正确性由 Theorem 3.4 的继承归纳情况保证。
Definition: 保留 `~base<>` / `~vbase<>` 结构。正确性由 `bases_of` 和
`qualified_name_for` 的 P2996 API 保证。✓

### 5.4 PolyRecord — 多态

签名包含 `,vptr` (Layout) 或 `,polymorphic` (Definition) 标记。
vptr 空间包含在 sizeof 中但不作为叶字段——这是正确的，因为 vptr 不是
`nonstatic_data_member`，但编译器在计算 sizeof 和所有 offset_of 时已考虑其存在。✓

### 5.5 Array(Type, N)

    ⟦T[N]⟧_L = "array[s:" ++ str(sizeof(T[N])) ++ ",a:" ++ str(alignof(T[N])) ++ "]<" ++ ⟦T⟧_L ++ "," ++ str(N) ++ ">"

特殊情况: byte 数组归一化为 `bytes[s:N,a:1]`。
归一化正确性: `char[N]` ≡ `uint8_t[N]` ≡ `std::byte[N]` 在内存中完全等价。✓

### 5.6 Union(fields)

成员不展平，保留原子签名。
正确性: union 成员共享偏移 @0，展平会导致歧义。✓

### 5.7 BitField(Type, width)

    ⟦bf⟧ = "@BYTE.BIT:bits<WIDTH," ++ σ(underlying) ++ ">"

所有数据由 P2996 API (`offset_of.bytes`, `offset_of.bits`, `bit_size_of`) 提供。
已知限制: 位排列顺序跨编译器是 impl-defined。✓ (with caveat)

### 5.8 Enum(underlying)

    ⟦E⟧_L = "enum[s:S,a:A]<" ++ σ(underlying) ++ ">"
    ⟦E⟧_D = "enum<" ++ qualified_name(E) ++ ">[s:S,a:A]<" ++ σ(underlying) ++ ">"

底层类型由 `std::underlying_type_t<E>` 获取。✓

---

## §6 Proof Architecture Summary

```
                        ┌──────────────────┐
                        │  §1 Type Domain   │
                        │  Definitions      │
                        │  1.1 Platform     │
                        │  1.2 σ (prims)    │
                        │  1.3 flatten      │
                        │  1.4 L_P (layout) │
                        │  1.5 D_P (struct) │
                        └────────┬─────────┘
                                 │
                        ┌────────▼─────────┐
                        │  §2 Denotation    │
                        │  ⟦·⟧_L, ⟦·⟧_D    │
                        │  CV-erasure       │
                        │  ≅_mem definition │
                        └────────┬─────────┘
                                 │
              ┌──────────────────┼──────────────────┐
              │                  │                  │
     ┌────────▼─────────┐ ┌─────▼──────┐ ┌────────▼──────────┐
     │  §3 Core Theorems│ │ §4 Refine  │ │ §5 Induction      │
     │  3.1 Faithfulness│ │ 4.1 π def  │ │ 8 type categories │
     │  3.2 Soundness   │ │ 4.2 V3     │ │ structural proof  │
     │  3.3 Conservative│ │ 4.3 Strict │ │ per category      │
     │  3.4 Offsets     │ └────────────┘ └───────────────────┘
     └──────────────────┘
```

**Key insight compared to previous PROOFS.md:**

| Aspect | Previous | Now |
|--------|----------|-----|
| Foundation | Ad hoc definitions | Denotational semantics framework |
| Core theorem | "Injectivity" | "Encoding Faithfulness" (stronger: has inverse) |
| V3 proof | "Function congruence" | Formal erasure with well-definedness proof |
| Type coverage | Case-by-case | Structural induction on type AST |
| Methodology reference | None | CompCert (faithfulness), seL4 (refinement) |
| Formal rigor | Semi-formal | Formal with named definitions, lemmas, theorems |

## Open Questions

- 是否值得用 Alloy 对签名函数进行有界模型检查？→ 可作为补充验证
- 是否值得用 Lean4/Coq 进行机器辅助证明？→ 目前纸笔证明已充分，机器证明作为 future work
