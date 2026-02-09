# Change: 分析每层签名的完备性与合理性

## Why
TypeLayout 声称两层签名系统能覆盖所有 C++ 类型的布局信息。但"完备"需要精确定义：
对于 C++ 类型系统中所有可能的结构差异，签名是否都能检测到？是否存在两个布局不同的
类型却产生相同签名的情况（false negative）？每个编码决策（如展平继承、忽略 CV 限定符）
是否合理？

## What Changes
- 定义"完备性"的精确含义：无漏报（Layout）、无漏报（Definition）
- 逐类型审计签名编码：基本类型、结构体、继承、union、enum、数组、位域、指针
- 识别已知不完备项，分析其是否为合理的设计选择
- 分析签名语法的无歧义性（能否从签名字符串唯一恢复类型属性）
- 评估每个编码决策的合理性

## Impact
- Affected specs: signature
- Affected code: `signature_detail.hpp`（所有 TypeSignature 特化）
- 为 Boost 评审提供"签名正确性"的完整论据
