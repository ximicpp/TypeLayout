# Change: Prove Two-Layer Signature Accuracy (Formal Analysis)

## Why
TypeLayout 的两层签名系统 (Layout / Definition) 是整个库的技术核心。
需要从数学和逻辑的角度严格证明签名的准确性——即签名函数是否正确地捕获了
它所承诺的信息，以及签名匹配是否真正蕴含所声明的性质。

## What Changes
- 形式化定义签名函数的语义 (Layout 签名作为字节身份函子, Definition 签名作为结构身份函子)
- 证明 Layout 签名的**单射性** (injectivity): 不同布局 → 不同签名
- 证明 Layout 签名匹配的**充分性** (soundness): 签名匹配 → 布局兼容
- 分析 Layout 签名的**非满射性**: 相同布局可能不同签名 (保守性)
- 证明 Definition → Layout 的**投影同态** (projection homomorphism)
- 证明 Definition 签名相对于 Layout 签名的**严格细化** (strict refinement)
- 对关键类型类别 (POD, 继承, 多态, 位域, 数组, union) 进行逐类别归纳证明
- 将形式化定义和定理记录到 signature spec

## Impact
- Affected specs: signature
- Affected code: 无代码变更，纯数学/逻辑分析
