## 1. 核心 Trait 实现
- [x] 1.1 创建 `serialization_traits.hpp` 文件
- [x] 1.2 实现 `is_serializable<T>` 编译期 trait
- [x] 1.3 实现 `has_pointer_member<T>` 递归检查（通过 `check_member_at_index` 实现）
- [x] 1.4 实现 `serialization_blocker<T>` 返回阻止序列化的原因

## 2. 序列化签名生成
- [x] 2.1 实现 `serialization_signature<T>()` 函数
- [x] 2.2 为可序列化类型生成带 `serial` 标记的签名
- [x] 2.3 为不可序列化类型生成带 `!serial:reason` 标记的签名
- [x] 2.4 递归检查嵌套类型的序列化能力

## 3. 平台约束系统（简化版）
- [x] 3.1 设计 `PlatformSet` 结构体（constexpr）
- [x] 3.2 实现预定义平台集（简化）：`bits64_le`, `bits64_be`, `bits32_le`, `bits32_be`, `current`
- [x] 3.3 支持字节序约束（little-endian, big-endian）
- [x] 3.4 支持位宽约束（32-bit, 64-bit）
- [x] 3.5 **始终拒绝 `long`/`unsigned long`**（跨平台 LLP64/LP64 差异）
- [x] 3.6 验证当前构建平台是否在目标平台集内
- [x] 3.7 实现平台约束的签名前缀生成 `[64-le]serial`

## 4. API 扩展
- [x] 4.1 在 `serialization_signature.hpp` 添加序列化相关 API
- [x] 4.2 添加 `serialization_signature<T>()` 函数
- [x] 4.3 添加 `check_serialization_compatible<T, U>()` 比较函数

## 5. 测试
- [x] 5.1 创建 `test/test_serialization_signature.cpp`
- [x] 5.2 测试基础可序列化类型（POD structs）
- [x] 5.3 测试不可序列化类型（含指针、多态等）
- [x] 5.4 测试嵌套类型的序列化检查
- [x] 5.5 测试平台约束功能

## 6. 文档
- [x] 6.1 更新 README 添加序列化签名说明
- [x] 6.2 添加使用示例
- [x] 6.3 说明 Layout vs Serialization 兼容性区别
