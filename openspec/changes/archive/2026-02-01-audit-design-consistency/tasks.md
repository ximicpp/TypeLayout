## 1. 删除旧 API，统一到 is_serializable_v<T, P>
- [x] 1.1 删除 `is_trivially_serializable<T>()` 函数
- [x] 1.2 删除 `is_trivially_serializable_v<T>` 变量模板
- [x] 1.3 删除 `TriviallySerializable<T>` concept
- [x] 1.4 删除 `is_portable<T>()` 废弃别名
- [x] 1.5 删除 `Portable<T>` 废弃 concept
- [x] 1.6 将位域检查添加到 `is_serializable_v<T, P>`

## 2. 重命名 serialization_signature 为 serialization_status
- [x] 2.1 重命名函数 `serialization_signature<T, P>()` → `serialization_status<T, P>()`
- [x] 2.2 更新 README 中的描述

## 3. 更新 project.md
- [x] 3.1 删除 `is_portable` 相关内容
- [x] 3.2 添加分层架构说明（Layer 1: Layout, Layer 2: Serialization）
- [x] 3.3 更新 Core API 表格使用 `is_serializable_v<T, P>`

## 4. 添加位域检查到新 API
- [x] 4.1 添加 `SerializationBlocker::HasBitField` 枚举值
- [x] 4.2 在 `basic_serialization_check` 中检测位域
- [x] 4.3 更新 `blocker_to_string` 添加 `!serial:bitfield`

## 5. 更新测试
- [x] 5.1 删除或更新 `test_portable_fix.cpp`
- [x] 5.2 更新 `test_serialization_signature.cpp` 测试位域
- [x] 5.3 更新 `test_all_types.cpp` 中的相关测试

## 6. 更新文档
- [x] 6.1 更新 README API 表格
- [x] 6.2 更新 docs/architecture.md（使用 is_serializable_v）
- [x] 6.3 更新 concepts.hpp 删除废弃 concepts

## 7. 清理（可选）
- [x] 7.1 考虑是否删除 portability.hpp（保留 has_bitfields）