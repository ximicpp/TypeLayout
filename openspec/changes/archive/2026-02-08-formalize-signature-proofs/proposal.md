# Change: Formalize Signature Proofs (Case Study + Rewrite)

## Why
当前 PROOFS.md 中的证明虽然逻辑正确，但形式化程度不够——缺乏对形式化证明方法论
本身的分析，也未参照已有的形式化验证案例。需要先研究形式化证明的标准做法，
然后据此重构 TypeLayout 的证明，使其达到更严谨的学术/工业标准。

## What Changes

### 第一阶段：分析形式化证明方法论和案例
- 调研学术界的形式化方法：Hoare Logic、Refinement Types、Bisimulation、Denotational Semantics
- 调研工业界的形式化验证案例：CompCert、seL4、TLA+、Alloy
- 提炼适用于"签名函数正确性"的形式化框架
- 对比各方法论的适用性和权衡

### 第二阶段：完整重写 TypeLayout 的形式化证明
- 基于分析结论选择最适合的形式化框架
- 用严格的数学定义和证明结构重写 PROOFS.md
- 确保每个定理都有完整的形式化陈述和证明
- 补充当前证明中缺少的形式化要素

## Impact
- Affected specs: signature
- Affected code: PROOFS.md (完整重写)
