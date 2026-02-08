# Change: Fix Proof Review Issues

## Why
PROOFS.md 中的形式化证明经审查发现 6 个需要修正的问题：
1. 定理 2.1 缺少签名字符串无歧义性的严格论证
2. 定义 1.2 中字段列表应为有序序列而非集合
3. 定理 4.1 的擦除函数 π 未证明良定义性
4. 定理 5.4 缺少 vptr 对 sizeof 影响的讨论
5. 缺少 CV 限定符处理说明
6. 定义 1.5 memcmp-兼容过于简化

## What Changes
- 新增 Lemma 1.7 (Grammar unambiguity) 证明签名语法无歧义
- 修正定义 1.2 从集合 `{}` 改为有序序列 `[]`，新增 `poly(T)` 分量
- 为定理 4.1 补充两个 Claim 论证 π 的确定性和正确性
- 为定理 5.4 补充 vptr 空间说明
- 新增定义 1.6 (CV-qualification erasure)
- 修正定义 1.5 补充字段数量和多态性条件
- 更新 signature spec 记录修正

## Impact
- Affected specs: signature
- Affected code: PROOFS.md (文档修正)
