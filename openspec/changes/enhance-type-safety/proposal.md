# Change: 增强类型安全性和文档清晰度

## Why

上一轮代码审查后，P0/P1 问题已全部清除，但仍存在 P2/P3 级别的改进空间：
1. Annotated 模式下字节数组丢失元素类型信息，降低调试体验
2. Structural 模式的语义定位不够明确（ABI 布局 vs 纯字节布局）
3. 内部头文件缺少保护机制，用户误用时错误信息不友好
4. 二级 concept 对不支持的类型可能触发硬错误

## What Changes

### 代码修改
- **type_signature.hpp**: Annotated 模式下保留字节数组元素类型（char/u8/byte）
- **type_signature.hpp + reflection_helpers.hpp**: 添加内部包含保护宏
- **concepts.hpp**: 为 `LayoutCompatible`、`LayoutHashCompatible`、`LayoutMatch`、`LayoutHashMatch` 添加 `LayoutSupported` 前置约束

### 文档修改
- **README.md**: 澄清 Structural 模式是 "ABI 布局"，包含继承语义信息

## Impact

- 受影响的 specs: core（类型签名、概念）
- 受影响的代码: type_signature.hpp, reflection_helpers.hpp, concepts.hpp, README.md
- **无破坏性变更**: 所有修改都是增强性的
