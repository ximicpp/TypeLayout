# Change: Remove Layer 2 (Serialization) from Specification

## Why

在审计核心功能完备性时发现，规范中定义的 **Layer 2 (Serialization Compatibility)** 功能完全未实现：

- `get_serialization_signature<T, PlatformSet>()` - 未实现
- `is_serializable<T, PlatformSet>` trait - 未实现  
- `PlatformSet` 平台集 - 未实现
- `serialization_blocker<T>` 诊断 - 未实现

**决策**: 简化规范，仅保留 Layer 1 (Layout Compatibility)，使规范与实现一致。

**理由**:
1. Layer 1 已完整可用，能满足核心用例（同进程/同平台布局验证）
2. 序列化兼容性是一个复杂话题，可能需要单独的库/模块
3. 保持核心库精简，符合 "Nano Architecture" 设计理念

## What Changes

从 `signature` 规范中移除以下内容：

1. **移除**: Two-Layer Architecture 中的 Layer 2 描述
2. **移除**: `get_serialization_signature<T>()` 相关要求
3. **移除**: Platform-Relative Serialization 相关要求
4. **移除**: `is_serializable<T>` trait 要求
5. **移除**: Serialization Blocker Diagnostic 要求
6. **移除**: PlatformSet 相关要求

## Impact

- 规范行数减少约 60%
- 规范与实现 100% 一致
- 核心库范围明确：仅提供 Layout 分析，不涉及序列化
