## 1. Review (已完成)
- [x] 1.1 逐节审查 §1-§6 的内部一致性
- [x] 1.2 对比实现代码验证每个定义
- [x] 1.3 识别 15 个问题 (5 High, 5 Medium, 5 Low)

## 2. 修复 High 级问题
- [x] 2.1 (P4+P7+P8) 扩展 ⟦·⟧_L 为完整的分情况定义，覆盖所有 8 个类型构造器；同步修正 flatten 中的 σ 为完整签名函数
- [x] 2.2 (P10) 修复 Conservativeness 反例：引入 ≅_byte vs ≅_mem 双层等价关系
- [x] 2.3 (P12) 修复 V3 投影证明：放弃 π 的纯字符串函数声称，改用语义层论证

## 3. 修复 Medium 级问题
- [x] 3.1 (P1+P2+P3) 扩展 σ 定义覆盖所有原始类型 (char/bool/byte/ref/rref/memptr/fnptr)
- [x] 3.2 (P5) 在 flatten 中增加 bit-field 分支
- [x] 3.3 (P11+P13) 将数组/联合/枚举 denotation 纳入 §2 正式定义

## 4. 修复 Low 级问题
- [x] 4.1 (P6) 补充语法 LL(1) 可解析论证（FIRST 集分析 + 左因子化）
- [x] 4.2 (P9) 明确 decode 为部分函数（dom(decode) = Im(⟦·⟧_L)）
- [x] 4.3 (P14+P15) 补充 complete type / anonymous enum / no_unique_address / declaration order 假设