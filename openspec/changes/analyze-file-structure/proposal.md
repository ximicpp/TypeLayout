# Change: 分析文件划分合理性

## Why
TypeLayout 经过多轮重构和清理后，核心代码集中在 5 个头文件中。需要从职责单一性、
依赖关系、可维护性等角度审视当前的文件划分是否合理，并提出优化方案（如有必要）。

## What Changes
- 纯分析报告，评估当前文件结构
- 如发现问题则提出重构方案

## Impact
- Affected specs: `signature`（若需调整文件结构）
- Affected code: `include/boost/typelayout/core/`（若需拆分/合并文件）
