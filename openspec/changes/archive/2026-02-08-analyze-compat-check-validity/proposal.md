# Change: 跨平台兼容检查方案的合理性、正确性与替代方案分析

## Why

Boost.TypeLayout 的跨平台兼容检查（Two-Phase Pipeline）是项目投入最多工程资源的应用场景。
在继续扩展该功能之前，需要严格审视：

1. **方案是否真正正确** — `layout_sig` 匹配是否 *充分* 保证 zero-copy 安全？
2. **设计是否合理** — Two-Phase 分离是否是最优架构？还是过度工程？
3. **是否存在更好的替代方案** — protobuf、FlatBuffers、DWARF 等方案的工程权衡如何？
4. **实际用户故事是否成立** — 声称的用例（IPC、mmap、network）在生产环境中是否可行？

这是一个**分析型 proposal**，主要产出为设计文档和改进建议，而非直接的代码变更。

## What Changes

- 新增全面的合理性与正确性分析报告 (`design.md`)
- 识别当前方案的**正确性边界**：哪些条件下签名匹配确实等于二进制兼容
- 识别当前方案的**正确性漏洞**：哪些场景下签名匹配但数据不兼容
- 对比主流替代方案的工程权衡
- 分析真实用户故事的可行性
- 根据分析结论提出改进建议（可能产生后续 change proposal）

## Impact

- Affected specs: `signature`（核心保证 V1 的精确边界）
- Affected code: 可能需要在文档/README 中明确声明正确性边界
- 后续可能产生: 改进型 change proposals（如增强 endian 处理、padding 显式化等）
