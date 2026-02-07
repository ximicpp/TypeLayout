## 1. P0 级修复（阻塞性问题）

- [x] 1.1 分析并确认平台类型特化问题
- [x] 1.2 用 `requires` 子句重写 `type_signature.hpp` 中的平台特化
- [x] 1.3 分析并确认 `CompileString::operator==` 越界问题
- [x] 1.4 修复 `compile_string.hpp` 中的比较逻辑

## 2. P1 级修复（严重问题）

- [x] 2.1 分析端序检测代码
- [x] 2.2 使用 `std::endian` 替代当前实现（config.hpp）
- [x] 2.3 分析 `char[]` 特殊处理
- [x] 2.4 统一所有单字节数组为 `bytes[]`（Structural 模式）

## 3. P2 级改进（性能/架构）

- [x] 3.1 减小 `from_number` 缓冲区大小（32→22）
- [x] 3.2 评估循环依赖问题 - 保持当前设计（前向声明足够）
- [x] 3.3 清理冗余的废弃函数（`format_size_align_mode` 已移除）

## 4. P3 级改进（代码风格）

- [x] 4.1 移除不必要的 `static` 关键字（reflection_helpers.hpp）
- [x] 4.2 评估 `void*` 特化 - 保留，需要解决偏序歧义
- [x] 4.3 添加 C++20 版本检查（config.hpp）

## 5. 测试验证

- [x] 5.1 本地构建验证 (Docker P2996 环境)
- [x] 5.2 运行所有测试 (8/8 通过)
