## 1. Root Cause Analysis (根因分析)
- [x] 1.1 分析100字段结构失败时的调用栈
  - 调用栈: `get_layout_signature` -> `get_layout_content_signature` -> `get_fields_signature` -> `concatenate_field_signatures` -> `build_field_with_comma` (100次折叠) -> `CompileString::operator+`
  - 瓶颈: 100个字段使用 fold expression 展开，每个字段签名逐个concatenate
- [x] 1.2 分析variant10失败时的调用栈
  - 调用栈类似，每个alternative类型生成子签名并concat
- [x] 1.3 确定步数消耗的主要来源
  - **主要来源**: `CompileString` 的 `operator+` 和构造函数中的逐字符复制循环
  - 签名长度 5183 字符 × 多次复制 = 大量循环迭代
- [x] 1.4 记录当前编译器默认步数限制
  - Bloomberg P2996 Clang默认步数限制约1,048,576 (~1M)

## 2. Compiler Flag Testing (编译器参数测试)
- [x] 2.1 测试 -fconstexpr-steps=2000000
  - **结果**: Large100 失败
- [x] 2.2 测试 -fconstexpr-steps=5000000
  - **结果**: Large100 + Variant10 都成功
- [x] 2.3 测试 -fconstexpr-steps=10000000
  - (跳过，5M已足够)
- [x] 2.4 确定支持100字段所需的最小步数
  - **精确边界**: 约 **2,190,000 步**
  - 2.18M失败，2.19M成功
  - **推荐配置**: 使用 `-fconstexpr-steps=3000000` (3M) 留安全余量

## 3. Implementation Optimization (实现优化)
- [x] 3.1 审查 concatenate_field_signatures 效率
  - 当前使用 fold expression `(build_field_with_comma<T, Indices, ...>() + ...)`
  - 问题: 左折叠导致 O(n²) 字符复制 (1+2+3+...+n)
  - 优化方案: 可考虑二分concat或预计算总长度后一次性构建
- [x] 3.2 审查 CompileString 操作效率
  - 每次 `operator+` 都创建新数组并逐字符复制
  - 这是constexpr的固有限制，没有move semantics
- [x] 3.3 评估是否可减少签名字符串长度
  - **已分析** - 详见下方"Signature Format Analysis"
  - **结论**: 保持现有格式，签名稳定性优先于长度优化
- [x] 3.4 评估是否可使用惰性计算
  - **结论**: 当前架构已是最直接的 consteval 实现，惰性计算会增加复杂度但收益有限

## 4. Documentation Update (文档更新)
- [x] 4.1 在project.md添加已知限制
- [x] 4.2 添加编译器参数建议
- [x] 4.3 记录测试结果和建议阈值

## Test Results Summary
| Test Case | Signature Length | Min Steps Required |
|-----------|-----------------|-------------------|
| Large60 (60 fields) | ~3110 chars | ~1M (default ok) |
| Large100 (100 fields) | 5183 chars | ~2.19M |
| Variant10 (10 types) | 5079 chars | ~2.19M (combined test) |

---

## Signature Format Analysis (签名格式分析)

### 签名长度组成

对于 100 个 `int32_t` 字段的结构体：
- 架构前缀: `[64-le]` = 7 chars
- 结构体包装: `struct[s:400,a:4]{}` ≈ 20 chars
- 每个字段: `@OFFSET[name]:i32[s:4,a:4],` ≈ 25 chars
- 总计: 7 + 20 + 100×25 ≈ 2527 chars (理论最小)
- 实际: 5183 chars (包含变长字段名)

### 发现的冗余

| 冗余类型 | 描述 | 潜在节省 |
|---------|------|---------|
| **重复 [s:SIZE,a:ALIGN]** | 每个 i32 字段带 `[s:4,a:4]` | 100 × 9 = 900 chars |
| **嵌套结构完整重复** | 相同类型结构签名多次出现 | 取决于结构 |

### 优化方案评估

| 方案 | 节省 | 评估 |
|------|------|------|
| 移除基本类型 [s:,a:] | ~17% | ❌ 破坏自描述性 |
| 类型引用 ($0, $1...) | 取决于结构 | ❌ 增加解析复杂度 |
| 顺序字段压缩 | 取决于结构 | ❌ 场景有限 |

### 决策

**保持现有签名格式不变**，理由：
1. **稳定性优先** - 现有用户的 `TYPELAYOUT_BIND` 断言依赖当前格式
2. **自描述性** - 人类可读是核心设计目标
3. **官方解决方案** - `-fconstexpr-steps` 是编译器支持的标准参数
4. **实际影响有限** - 绝大多数项目不会有 100+ 字段的结构体

### 分析文件

测试代码: `test/analyze_signature.cpp`
