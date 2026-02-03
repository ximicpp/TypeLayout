# Change: 添加竞品分析与价值定位文档

## Why

TypeLayout 需要一份系统性的竞品分析和价值定位文档，用于：

1. **CppCon 演讲审核** - 审稿人需要了解这个方案相比现有方案的创新点
2. **Boost 库审核** - Boost 社区关注"是否填补了空白"和"是否有足够的差异化"
3. **学术/技术文章** - 明确技术贡献和创新点
4. **项目推广** - 清晰的价值主张

目前 README 中有简单的对比表，但缺乏深入分析。

## What Changes

- 新增 `doc/analysis/competitive_analysis.md` - 竞品调研与对比
- 新增 `doc/analysis/value_proposition.md` - 价值定位与创新点
- 新增 `doc/analysis/reviewer_pitch.md` - 针对不同审核人员的要点

## Impact

- Affected specs: `documentation`
- Affected code: 无代码变更，仅文档
- 无破坏性变更

## 调研范围

### 竞品类别

| 类别 | 方案 | 关系 |
|------|------|------|
| **序列化框架** | Protobuf, FlatBuffers, Cap'n Proto, MessagePack | 解决类似问题，但方法不同 |
| **反射库** | Boost.PFR, Magic Enum, refl-cpp | 技术基础相似 |
| **ABI 工具** | libabigail, abi-compliance-checker | 目标相似，方法不同 |
| **类型安全** | Boost.Hana, Boost.TypeIndex | 概念相关 |
| **P2996 应用** | （调研中） | 直接竞争/互补 |

### 分析维度

1. **技术能力对比** - 能做什么/不能做什么
2. **使用复杂度** - 学习曲线、集成成本
3. **运行时开销** - 性能影响
4. **生态系统** - 社区、工具链、文档
5. **适用场景** - 各自的最佳应用场景

## 目标受众分析

### CppCon 审稿人

- 关注：技术创新、实用性、演讲吸引力
- 期望：看到 P2996 的"杀手级应用"
- 痛点：听过太多"又一个序列化库"的提案

### Boost 审核委员会

- 关注：是否填补空白、设计质量、测试覆盖、文档完整性
- 期望：符合 Boost 设计哲学（零开销、通用性）
- 痛点：担心与现有库重叠、维护负担

### 学术/技术社区

- 关注：技术贡献、可复现性、理论基础
- 期望：明确的创新点和适用边界
- 痛点：过度营销、缺乏严谨性
