## 1. 性能分析与设计
- [x] 1.1 分析当前实现的 constexpr 步数消耗点
- [x] 1.2 确定根本原因：每个成员都单独调用 nonstatic_data_members_of()
- [x] 1.3 理解 P2996 工具链的 constexpr 堆分配限制

## 2. 增量哈希实现（方案 B）
- [x] 2.1 实现 `FNV1aState` constexpr 哈希状态类（在 utils/hash.hpp）
- [x] 2.2 发现 P2996 工具链限制：constexpr 中无法缓存 vector 结果
- [x] 2.3 决定保留 FNV1aState 为未来优化做准备

## 3. `template for` 优化（方案 C）
- [x] 3.1 尝试使用 template for 循环减少反射 API 调用
- [x] 3.2 发现核心问题：template for 同样无法处理堆分配的 vector
- [x] 3.3 错误信息：`pointer to subobject of heap-allocated object is not a constant expression`
- [x] 3.4 结论：此方案在当前工具链下不可行

## 4. 当前工具链限制文档化
- [x] 4.1 在 reflection_helpers.hpp 添加详细限制说明
- [x] 4.2 在 test_stress.cpp 添加详细限制说明
- [x] 4.3 调整测试目标到可支持的成员数量（40个）
- [x] 4.4 更新 proposal.md 记录完整分析

## 5. 测试与验证
- [x] 5.1 创建 20 成员压力测试结构体（基线）
- [x] 5.2 创建 30 成员压力测试结构体
- [x] 5.3 创建 40 成员压力测试结构体（当前限制）
- [x] 5.4 验证所有压力测试编译通过
- [x] 5.5 验证签名/哈希值正确性
- [x] 5.6 验证 hashes_match 对大结构体工作正常

## 6. 文档与收尾
- [x] 6.1 文档说明成员数量限制（约40-50成员）
- [x] 6.2 说明这是 P2996 工具链限制，非库设计限制
- [x] 6.3 将 hash.hpp 从 core/ 移动到 utils/（架构优化）

## 7. CMake 配置与签名优化
- [x] 7.1 添加 CMake 可配置选项 `BOOST_TYPELAYOUT_CONSTEXPR_STEPS`
- [x] 7.2 创建签名大小测试文件 `test/test_signature_size.cpp`
- [x] 7.3 验证 Structural 模式下 100 成员签名仅需 ~1800 字符（减少 65%）
- [x] 7.4 更新 project.md 文档反映最新测试结果

## 结论

**当前 P2996 工具链（Bloomberg 实验性 Clang）存在根本性限制**：

| 问题 | 原因 |
|------|------|
| 结构体成员数量限制约 40-50 个 | 反射 API 返回 `std::vector` 导致每次调用都有大量 constexpr 步数开销 |
| `template for` 无法使用 | 堆分配的 vector 不能作为 constexpr 范围表达式 |
| 无法缓存成员列表 | 堆分配指针不能跨 constexpr 函数调用存储 |

**工作成果**：

1. ✅ 添加了 `FNV1aState` 流式哈希辅助类（为未来优化准备）
2. ✅ 创建了压力测试套件来验证和追踪支持的成员数量
3. ✅ 深度分析并文档化了工具链限制
4. ✅ 验证 40 成员结构体可以正常工作
5. ✅ 优化了项目架构（hash.hpp 移至 utils/）
6. ✅ 添加 CMake 可配置的 `-fconstexpr-steps` 选项
7. ✅ 签名优化：Structural 模式下每成员仅 ~18 字符（减少 65%）

**签名大小改进（Structural 模式 - 无字段名）**：

| 成员数 | 签名长度 | 每成员字符数 |
|-------|---------|------------|
| 20 | 361 | 18.1 |
| 40 | 717 | 17.9 |
| 60 | 1077 | 17.9 |
| 80 | 1437 | 18.0 |
| 100 | 1797 | 18.0 |

**CMake 配置**：
```bash
# 支持 100+ 成员的大型结构体
cmake -DBOOST_TYPELAYOUT_CONSTEXPR_STEPS=5000000 ..
```

**未来改进路径**：

当 P2996 工具链改进后（例如支持 constexpr 范围迭代器或优化反射 API 的步数消耗），
可以：
1. 重新实现 `template for` 方案
2. 或实现增量哈希方案

来支持 200+ 成员的结构体。
