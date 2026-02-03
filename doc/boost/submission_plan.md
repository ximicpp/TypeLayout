# Boost Library Submission Plan: TypeLayout

## Executive Summary

本文档概述了将 TypeLayout 提交到 Boost C++ Libraries 的完整计划，包括入库流程、当前差距分析、改进计划和时间线。

---

## Part 1: Boost 入库流程

### 1.1 Boost Review Process 概述

Boost 库的入库需要经历以下阶段：

```
1. Development → 2. Review Wizard → 3. Formal Review → 4. Acceptance
      ↓                 ↓                  ↓                ↓
   完善库           提交请求          社区评审         正式入库
```

### 1.2 详细流程

#### Stage 1: Pre-Submission 准备

| 要求 | 说明 |
|------|------|
| **Boost License** | 必须使用 Boost Software License 1.0 |
| **Header-only 或 Build 支持** | 需要支持 B2 (Boost.Build) 或 CMake |
| **文档** | 需要 Quickbook/AsciiDoc 格式的完整文档 |
| **测试** | 需要全面的测试套件，使用 Boost.Test |
| **示例** | 需要可编译运行的示例代码 |

#### Stage 2: Review Wizard 申请

1. 在 Boost 邮件列表宣布意图（boost-announce）
2. 联系 Review Wizard 申请 Formal Review
3. 提供：
   - 库描述（一句话 + 详细说明）
   - 文档链接
   - 源码仓库链接

#### Stage 3: Formal Review（正式评审）

- 持续时间：通常 10 个工作日
- 评审者在邮件列表发表意见
- 必须回答以下问题：
  1. 库的设计和实现质量如何？
  2. 库的文档质量如何？
  3. 库的用途有多广泛？
  4. 库是否遵循 Boost 最佳实践？
  5. 你是否投票支持入库？

#### Stage 4: Review Manager 决定

Review Manager 综合评审意见，做出以下决定之一：
- **Accept** - 直接入库
- **Conditional Accept** - 需要修改后入库
- **Reject** - 拒绝入库

### 1.3 Boost 库的技术要求

| 类别 | 具体要求 |
|------|----------|
| **命名空间** | 必须使用 `boost::` 前缀 |
| **头文件** | 位于 `boost/library_name/` 目录 |
| **宏** | 必须使用 `BOOST_LIBRARY_NAME_` 前缀 |
| **异常** | 遵循 Boost.Exception 模式或提供 noexcept 保证 |
| **依赖** | 尽量减少对其他 Boost 库的依赖 |
| **可移植性** | 支持主流编译器（GCC, Clang, MSVC） |
| **C++ 标准** | 明确最低 C++ 版本要求 |

### 1.4 成功入库案例分析：Boost.PFR

Boost.PFR（Precise and Flat Reflection）是一个类似的反射库，于 2021 年成功入库。

**关键成功因素：**
- 明确的用途：结构体字段反射
- 简洁的 API：`boost::pfr::for_each_field`
- 无侵入设计：不需要修改用户类型
- 全面的测试：覆盖各种边界情况
- 清晰的文档：包含教程、参考、设计原理

**TypeLayout 可借鉴点：**
- API 设计风格
- 文档结构
- 测试组织方式

---

## Part 2: 差距分析

### 2.1 TypeLayout 当前状态

| 方面 | 当前状态 | 说明 |
|------|----------|------|
| License | ⚠️ 未明确 | 需要添加 BSL-1.0 |
| 命名空间 | ⚠️ `boost::typelayout` | 符合要求 |
| 构建系统 | ⚠️ CMake only | 需要添加 B2 支持 |
| 文档 | ⚠️ Markdown | 需要转换为 Quickbook/AsciiDoc |
| 测试 | ⚠️ 基础测试 | 需要扩展，使用 Boost.Test |
| 示例 | ✅ 已有 | 需要确保可编译 |
| CI/CD | ⚠️ 基础 | 需要多编译器测试 |

### 2.2 关键差距

#### 🔴 Critical（必须解决）

1. **License 文件**
   - 添加 `LICENSE_1_0.txt`（Boost Software License）
   - 在所有源文件添加 license header

2. **文档格式**
   - 将 Markdown 转换为 Quickbook 或 AsciiDoc
   - 包含：Introduction, Tutorial, Reference, Design Rationale

3. **测试框架**
   - 迁移到 Boost.Test
   - 添加更多边界测试

#### 🟡 Important（强烈建议）

4. **B2 构建支持**
   - 添加 `Jamfile.v2`
   - 与 CMake 并存

5. **多编译器 CI**
   - GCC 11/12/13
   - Clang 14/15/16
   - MSVC 19.3+

#### 🟢 Nice-to-have

6. **Boost 依赖**
   - 考虑是否使用 Boost.Config 等基础库
   - 保持依赖最小化

---

## Part 3: 入库计划

### 3.1 阶段规划

```
Phase 1: Foundation (4 weeks)
├── License 和 Copyright
├── 文档框架
└── 测试框架迁移

Phase 2: Quality (4 weeks)
├── 文档完善
├── 测试覆盖
└── CI 多编译器

Phase 3: Integration (2 weeks)
├── B2 构建支持
├── Boost 目录结构
└── 最终审查

Phase 4: Submission (2-4 weeks)
├── 邮件列表宣布
├── Review Wizard 申请
└── Formal Review
```

### 3.2 详细任务

#### Phase 1: Foundation（基础建设）

| Task | 优先级 | 预估时间 |
|------|--------|----------|
| 添加 BSL-1.0 License | P0 | 1 day |
| 源文件添加 license header | P0 | 1 day |
| 创建 Quickbook 文档框架 | P0 | 3 days |
| 迁移测试到 Boost.Test | P0 | 3 days |

#### Phase 2: Quality（质量提升）

| Task | 优先级 | 预估时间 |
|------|--------|----------|
| 编写 Introduction 章节 | P1 | 2 days |
| 编写 Tutorial 章节 | P1 | 3 days |
| 编写 Reference 章节 | P1 | 3 days |
| 编写 Design Rationale | P1 | 2 days |
| 添加边界测试 | P1 | 3 days |
| 配置 CI 多编译器 | P1 | 2 days |

#### Phase 3: Integration（集成准备）

| Task | 优先级 | 预估时间 |
|------|--------|----------|
| 添加 Jamfile.v2 | P2 | 2 days |
| 调整目录结构 | P2 | 1 day |
| 内部代码审查 | P2 | 2 days |

#### Phase 4: Submission（提交）

| Task | 优先级 | 预估时间 |
|------|--------|----------|
| 在 boost-announce 宣布 | P0 | 1 day |
| 联系 Review Wizard | P0 | 1 day |
| 准备 Review 材料 | P0 | 2 days |
| Formal Review 响应 | P0 | 10 days |

### 3.3 时间线

```
2026 Q1: Phase 1 + Phase 2
2026 Q2: Phase 3 + Phase 4 开始
2026 Q3: Formal Review + 入库
```

### 3.4 风险和应对

| 风险 | 可能性 | 影响 | 应对策略 |
|------|--------|------|----------|
| P2996 未进入 C++26 | 低 | 高 | 保持与 Bloomberg Clang 兼容 |
| Review 被拒 | 中 | 高 | 提前在邮件列表获取反馈 |
| 文档工作量超预期 | 中 | 中 | 优先核心文档，迭代改进 |
| 测试覆盖不足 | 低 | 中 | 使用覆盖率工具监控 |

---

## Part 4: 立即行动项

### 短期（1 周内）

- [ ] 添加 `LICENSE_1_0.txt`
- [ ] 在 README 中声明 License
- [ ] 创建 `doc/boost/` 目录结构

### 中期（1 月内）

- [ ] 完成 Quickbook 文档框架
- [ ] 迁移至少 50% 测试到 Boost.Test
- [ ] 配置 GitHub Actions 多编译器

### 长期（3 月内）

- [ ] 完成全部文档
- [ ] 完成 B2 构建支持
- [ ] 在 boost-users 列表获取早期反馈

---

## 参考资料

- [Boost Library Submission Process](https://www.boost.org/development/submissions.html)
- [Boost Review Wizard](https://www.boost.org/community/review_schedule.html)
- [Boost Library Requirements](https://www.boost.org/development/requirements.html)
- [Boost.PFR GitHub](https://github.com/boostorg/pfr)
- [Writing Boost Documentation](https://www.boost.org/doc/libs/develop/doc/html/quickbook.html)
