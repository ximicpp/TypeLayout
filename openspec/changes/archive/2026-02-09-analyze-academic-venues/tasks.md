## 1. 学术会议/期刊分析
- [x] 1.1 提炼 TypeLayout 的学术研究贡献
- [x] 1.2 评估 PL 顶会匹配度
- [x] 1.3 评估 SE/Systems 会议匹配度
- [x] 1.4 评估期刊匹配度
- [x] 1.5 制定投稿策略和论文章节规划

---

## 2. TypeLayout 学术投稿目标分析

---

### 2.1 TypeLayout 的学术研究贡献提炼

在评估适合的会议/期刊之前，首先需要明确 TypeLayout 的核心学术贡献——这决定了论文的定位和目标受众。

#### 核心贡献 (Research Contributions)

| # | 贡献 | 学术领域 | 新颖性 |
|---|------|---------|--------|
| C1 | **编译时类型布局签名系统** — 利用 C++26 静态反射（P2996）在编译时生成完整的类型内存布局签名 | PL / Metaprogramming | 🟢 高 — 首个将 P2996 应用于布局验证的系统 |
| C2 | **两层签名的形式化语义** — 基于指称语义和细化理论的双层签名系统，包含 Soundness、Injectivity、Projection 定理 | PL / Formal Methods | 🟢 高 — C++ 库中极罕见的形式化工作 |
| C3 | **跨平台布局验证工具链** — Phase 1 (P2996 导出) + Phase 2 (C++17 比对) 的两阶段架构 | SE / Tooling | 🟡 中 — 工程贡献，学术新颖性一般 |
| C4 | **结构等价 vs 字节等价的分层理论** — Definition (structural identity) vs Layout (byte identity) 的形式化区分 | PL / Type Theory | 🟢 高 — 提供了一个关于"类型何时可安全替换"的新理论框架 |
| C5 | **零开销编译时 ABI 验证** — 所有检查在编译时完成，零运行时开销 | PL / Compiler Technology | 🟡 中 — 编译时检查本身不新，但应用于 ABI 验证是新的 |

#### 论文核心叙事 (Paper Narrative)

> **一句话定位**: 我们提出一种基于 C++26 静态反射的编译时类型布局签名系统，
> 通过两层签名（字节身份 / 结构身份）及其形式化语义，实现了零开销、自动化的跨编译单元和跨平台类型兼容性验证。

**关键学术主张 (Claims)**:
1. Layout 签名是**可靠的 (sound)**：签名匹配 ⟹ memcmp 兼容
2. Layout 签名是**单射的 (injective)**：不同布局 ⟹ 不同签名
3. Definition 签名是 Layout 签名的**严格细化 (strict refinement)**
4. 该系统可在编译时完全验证，**零运行时开销**
5. 两阶段工具链使验证可**跨平台**进行

---

### 2.2 PL 领域顶级会议评估

#### Tier 1 — 顶级会议 (CCF-A / CORE A*)

| 会议 | 全称 | 匹配度 | 分析 |
|------|------|--------|------|
| **OOPSLA** | Object-Oriented Programming, Systems, Languages, and Applications | ⭐⭐⭐⭐⭐ | **最佳目标。** OOPSLA 欢迎"语言特性的新应用"和"类型系统扩展"。TypeLayout 的两层签名系统、P2996 应用、形式化证明完美契合。历史上接受过类似的反射/元编程工具论文。 |
| **PLDI** | Programming Language Design and Implementation | ⭐⭐⭐⭐ | **很好的目标。** PLDI 偏重"实现"和"编译器技术"。TypeLayout 的编译时签名生成可以从编译器技术角度展示。但 PLDI 更偏好有性能评估的系统论文。 |
| **POPL** | Principles of Programming Languages | ⭐⭐⭐ | **匹配度一般。** POPL 极度偏重理论。TypeLayout 的形式化证明有价值，但指称语义的深度可能不够 POPL 的理论标准。需要将证明提升到更抽象的层次。 |
| **ECOOP** | European Conference on Object-Oriented Programming | ⭐⭐⭐⭐⭐ | **极好的目标。** ECOOP 关注 OOP 类型系统、反射、元编程。TypeLayout 的继承处理、多态类型签名、结构分析 vs 名义分析的理论框架非常契合。 |
| **CC** | International Conference on Compiler Construction | ⭐⭐⭐⭐ | **好的目标。** CC 关注编译器技术和编译时分析。P2996 的编译时签名生成是一个自然的 CC 话题。 |

#### Tier 2 — 高质量会议 (CCF-B / CORE A)

| 会议 | 全称 | 匹配度 | 分析 |
|------|------|--------|------|
| **SLE** | Software Language Engineering | ⭐⭐⭐⭐⭐ | **极好的目标。** SLE 专门关注语言工程、DSL、语言工具。TypeLayout 的签名系统作为一种"编译时领域语言"完美匹配。对首次投稿者友好。 |
| **GPCE** | Generative Programming: Concepts & Experiences | ⭐⭐⭐⭐⭐ | **极好的目标。** GPCE 专注于生成式编程和元编程。P2996 反射 + 编译时签名生成 = 完美匹配。 |
| **CGO** | Code Generation and Optimization | ⭐⭐⭐ | 偏重代码生成优化，匹配度较低。 |
| **DLS** | Dynamic Languages Symposium | ⭐⭐ | 关注动态语言，不匹配。 |
| **ISMM** | International Symposium on Memory Management | ⭐⭐⭐⭐ | TypeLayout 涉及内存布局，有一定匹配度。 |

#### Tier 3 — Workshop 级别 (适合首次投稿)

| 会议 | 全称 | 匹配度 | 分析 |
|------|------|--------|------|
| **C++Now** | — | ⭐⭐⭐⭐⭐ | 专门的 C++ 技术会议，可提交技术报告。 |
| **ARRAY** | Workshop on Libraries, Languages, and Compilers for Array Programming | ⭐⭐⭐ | 偏数组，但 TypeLayout 的数组签名处理有一定相关性。 |
| **META** | Workshop on Meta-Programming Techniques and Reflection | ⭐⭐⭐⭐⭐ | 如果存在此 workshop，是完美匹配。 |

---

### 2.3 SE / Systems 会议评估

| 会议 | 匹配度 | 分析 |
|------|--------|------|
| **ICSE** (SEIP Track) | ⭐⭐⭐⭐ | ICSE 的 Software Engineering in Practice track 接受工具论文。跨平台验证工具链是一个好的工业应用故事。 |
| **FSE** (Tool Demo) | ⭐⭐⭐⭐ | FSE 的 Tool Demo track 非常适合 TypeLayout 的演示。 |
| **ASE** | ⭐⭐⭐ | 自动化软件工程角度可行，但匹配度不如 PL 会议。 |
| **USENIX ATC** | ⭐⭐⭐ | 系统工具论文，但 ATC 偏重大规模系统评估。 |

---

### 2.4 期刊评估

| 期刊 | 全称 | 级别 | 匹配度 | 分析 |
|------|------|------|--------|------|
| **TOPLAS** | ACM Trans. on Programming Languages and Systems | CCF-A | ⭐⭐⭐⭐⭐ | **最佳期刊目标。** TOPLAS 接受 PL 系统论文，TypeLayout 的形式化证明 + 系统实现完美匹配。审稿周期长（6-12 月）。 |
| **SCP** | Science of Computer Programming | CCF-B | ⭐⭐⭐⭐ | 接受编程语言和形式化方法论文。适合 TypeLayout 的形式化语义部分。 |
| **JSS** | Journal of Systems and Software | CCF-B | ⭐⭐⭐ | 偏软件工程，适合工具链部分。 |
| **SPE** | Software: Practice and Experience | CCF-B | ⭐⭐⭐⭐ | 接受"实践性强的系统论文"，TypeLayout 的工程质量和实用性契合。 |
| **JFP** | Journal of Functional Programming | — | ⭐⭐ | 偏函数式编程，不太匹配。 |

---

### 2.5 匹配度排名总结

#### 🏆 Top 5 推荐目标（按优先级排序）

| 排名 | 目标 | 类型 | 匹配度 | 投稿策略 |
|------|------|------|--------|---------|
| 1 | **OOPSLA** | 顶会 (CCF-A) | ⭐⭐⭐⭐⭐ | 完整论文，强调两层签名理论 + P2996 应用 + 形式化证明 |
| 2 | **ECOOP** | 顶会 (CCF-A) | ⭐⭐⭐⭐⭐ | 完整论文，强调 OOP 类型系统、继承处理、结构 vs 名义分析 |
| 3 | **SLE** | 高质量会议 (CCF-B) | ⭐⭐⭐⭐⭐ | 完整论文，强调语言工程和签名 DSL 设计 |
| 4 | **GPCE** | 高质量会议 (CCF-B) | ⭐⭐⭐⭐⭐ | 完整论文，强调生成式编程和 P2996 元编程 |
| 5 | **PLDI** | 顶会 (CCF-A) | ⭐⭐⭐⭐ | 完整论文，强调编译器技术和编译时验证 |

#### 📋 备选/降档目标

| 目标 | 策略 |
|------|------|
| **TOPLAS** (期刊) | OOPSLA/ECOOP 投稿被拒后的期刊投稿路径，可以写更长更详细的版本 |
| **CC** | 如果 PLDI 被拒，从编译器技术角度重新包装 |
| **ICSE SEIP** | 强调工业实践和跨平台工具链 |
| **FSE Tool Demo** | 4 页工具演示论文，门槛低，适合首次投稿 |

---

### 2.6 论文结构规划

基于 OOPSLA/ECOOP 的论文格式（12-25 页），规划以下章节：

| 章节 | 标题 | 页数 | 内容 |
|------|------|------|------|
| §1 | **Introduction** | 2 | 问题动机、Before/After 对比、贡献列表 |
| §2 | **Background** | 1.5 | C++ 内存布局模型、P2996 反射 API、现有方案的局限 |
| §3 | **The TypeLayout Signature System** | 3 | 两层签名的设计、签名语法、生成算法 |
| §4 | **Formal Semantics** | 3 | 指称语义框架、Soundness/Injectivity/Projection 定理及证明 |
| §5 | **Cross-Platform Toolchain** | 1.5 | Phase 1/Phase 2 架构、.sig.hpp 格式 |
| §6 | **Evaluation** | 3 | 类型覆盖率、签名正确性验证、编译时开销、与现有方案对比 |
| §7 | **Related Work** | 1.5 | ABI Checker、Boost.PFR、RTTI、Protobuf、seL4 形式化验证 |
| §8 | **Discussion & Limitations** | 1 | 已知限制、未来工作 |
| §9 | **Conclusion** | 0.5 | 总结 |
|    | **Total** | **~17 页** | |

#### 每章节对应的 Proposal

| Proposal ID | 章节 | 说明 |
|-------------|------|------|
| `write-paper-sec1-introduction` | §1 Introduction | 问题动机和贡献声明 |
| `write-paper-sec2-background` | §2 Background | 技术背景和现有方案 |
| `write-paper-sec3-system` | §3 Signature System | 核心系统设计 |
| `write-paper-sec4-formal` | §4 Formal Semantics | 形式化证明 |
| `write-paper-sec5-toolchain` | §5 Cross-Platform Toolchain | 跨平台工具链 |
| `write-paper-sec6-evaluation` | §6 Evaluation | 实验评估 |
| `write-paper-sec7-related` | §7 Related Work | 相关工作 |
| `write-paper-sec8-conclusion` | §8-9 Discussion & Conclusion | 讨论和总结 |

---

### 2.7 投稿时间线建议

```
2026 Q1 (现在):  完成论文初稿（所有章节）
2026 Q2 (4月):   → OOPSLA 2026 第二轮投稿（截止日通常在 4 月中）
                  → 或 ECOOP 2026（截止日通常在 1-2 月，可能已过）
2026 Q3 (7月):   如果 OOPSLA 被拒 → OOPSLA 第三轮（截止日通常在 10 月）
                  → 或改投 SLE/GPCE 2026
2026 Q4 (10月):  → OOPSLA 第三轮 / TOPLAS 期刊投稿
2027 Q1:         → PLDI 2027 / ECOOP 2027
```

> **注**: OOPSLA 现在采用滚动审稿（3 轮/年），每轮截止日约在 1月、4月、10月。
> 这是一个巨大的优势——被拒后可以快速修改并投下一轮。

---

### 2.8 录用难度评估

| 目标 | 录用率 | TypeLayout 匹配度 | 预估录用概率 |
|------|--------|-------------------|-------------|
| OOPSLA | ~25% | ⭐⭐⭐⭐⭐ | 30-40% |
| ECOOP | ~25% | ⭐⭐⭐⭐⭐ | 30-40% |
| PLDI | ~20% | ⭐⭐⭐⭐ | 20-30% |
| SLE | ~30% | ⭐⭐⭐⭐⭐ | 40-50% |
| GPCE | ~35% | ⭐⭐⭐⭐⭐ | 45-55% |
| TOPLAS | ~20% | ⭐⭐⭐⭐⭐ | 25-35% |

**关键提升因素**:
- 形式化证明是 PL 会议的"硬通货"
- P2996 是 2026 年的热门话题
- 两层签名理论有真正的学术新颖性
- 有可运行的完整实现（不是 paper prototype）

**关键风险**:
- PL 顶会评审对"工具论文"有偏见，需要足够的理论深度
- 评估部分需要与现有方案有定量对比（目前缺失）
- 需要明确说明"这不仅仅是一个 C++ 库，而是一个新的类型验证理论"
