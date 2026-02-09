# documentation Specification

## Purpose
TBD - created by archiving change add-boost-antora-docs. Update Purpose after archive.
## Requirements
### Requirement: Antora 文档站点配置
系统 SHALL 提供 Antora 站点配置文件，支持构建完整的文档站点。

#### Scenario: 成功构建文档站点
- **GIVEN** 项目包含有效的 `antora.yml` 和 `antora-playbook.yml`
- **WHEN** 执行 `npx antora antora-playbook.yml`
- **THEN** 成功生成静态 HTML 文档站点
- **AND** 所有页面可正常导航

### Requirement: 用户指南文档
系统 SHALL 提供完整的用户指南，涵盖库的所有主要功能。

#### Scenario: 用户指南包含核心功能
- **GIVEN** 用户访问用户指南
- **WHEN** 浏览文档目录
- **THEN** 应包含以下章节:
  - 布局签名 (Layout Signatures)
  - 类型支持 (Type Support)
  - 可移植性检查 (Portability)
  - 模板约束 (Concepts)
  - 哈希验证 (Hash Verification)
  - 继承支持 (Inheritance)
  - 位域支持 (Bitfields)

#### Scenario: 用户指南示例可编译
- **GIVEN** 用户指南中的任意代码示例
- **WHEN** 使用支持 P2996 的编译器编译
- **THEN** 代码成功编译无错误

### Requirement: API 参考文档

系统 SHALL 提供完整的 API 参考文档，描述所有公开接口。所有公共 API 包含 Doxygen 风格的详细文档，包括参数说明、返回值和使用示例。

#### Scenario: 开发者查阅 API 文档

- **WHEN** 开发者打开 API 参考文档
- **THEN** 显示所有公共函数的详细说明
- **AND** 每个函数包含参数描述
- **AND** 每个函数包含返回值描述
- **AND** 提供使用示例代码

#### Scenario: IDE 智能提示

- **WHEN** 开发者在 IDE 中使用 TypeLayout API
- **THEN** IDE 显示 Doxygen 注释作为智能提示
- **AND** 显示参数和返回值类型信息

### Requirement: 设计原理文档
系统 SHALL 提供设计原理 (Rationale) 文档，解释关键设计决策。

#### Scenario: 设计原理解释签名格式
- **GIVEN** 设计原理文档
- **WHEN** 查看签名格式章节
- **THEN** 应解释:
  - 为什么选择当前签名格式
  - 签名格式的设计考量
  - 与其他方案的对比

#### Scenario: 设计原理解释哈希算法
- **GIVEN** 设计原理文档
- **WHEN** 查看哈希算法章节
- **THEN** 应解释:
  - 为什么选择 FNV-1a 和 DJB2
  - 双哈希验证的原理
  - 碰撞概率分析

### Requirement: 配置选项文档
系统 SHALL 提供配置选项文档，说明编译器和平台要求。

#### Scenario: 配置文档说明编译器选项
- **GIVEN** 配置选项文档
- **WHEN** 查看编译器选项章节
- **THEN** 应包含:
  - 必需的编译器标志 (`-freflection`)
  - 支持的编译器列表
  - 最低 C++ 标准要求

#### Scenario: 配置文档说明平台支持
- **GIVEN** 配置选项文档
- **WHEN** 查看平台支持章节
- **THEN** 应包含:
  - 支持的架构 (32-bit, 64-bit)
  - 支持的字节序 (little-endian, big-endian)
  - 平台检测宏说明

### Requirement: 示例代码文档
系统 SHALL 提供实用的示例代码，演示常见使用场景。

#### Scenario: 示例代码包含常见场景
- **GIVEN** 示例代码文档
- **WHEN** 浏览示例目录
- **THEN** 应包含以下场景的示例:
  - 基本使用
  - 网络协议验证
  - 共享内存验证
  - 文件格式验证
  - 插件/DLL 接口验证

#### Scenario: 所有示例代码可编译
- **GIVEN** `example/` 目录下的所有 `.cpp` 文件
- **WHEN** 使用 CMake 构建示例目标
- **THEN** 所有示例成功编译

#### Scenario: 插件接口示例演示 ABI 验证
- **GIVEN** 插件接口验证示例
- **WHEN** 用户运行示例
- **THEN** 示例演示插件加载时的布局哈希验证
- **AND** 示例演示布局不匹配时的检测和错误处理

### Requirement: 文档导航结构
系统 SHALL 提供清晰的文档导航结构。

#### Scenario: 导航包含所有主要章节
- **GIVEN** 文档导航 (`nav.adoc`)
- **WHEN** 查看导航结构
- **THEN** 应包含:
  - 概述 (Overview)
  - 快速入门 (Quick Start)
  - 用户指南 (User Guide)
  - API 参考 (Reference)
  - 设计原理 (Design)
  - 配置选项 (Configuration)
  - 示例 (Examples)

### Requirement: 文档格式符合 Boost 标准
系统 SHALL 使用 Asciidoc 格式，符合现代 Boost 库文档标准。

#### Scenario: 文档使用 Asciidoc 格式
- **GIVEN** `doc/modules/ROOT/pages/` 目录
- **WHEN** 检查文件格式
- **THEN** 所有文档文件使用 `.adoc` 扩展名

#### Scenario: 文档使用 Antora 结构
- **GIVEN** `doc/` 目录
- **WHEN** 检查目录结构
- **THEN** 应符合 Antora 标准结构:
  - `antora.yml` 存在于 `doc/` 根目录
  - `modules/ROOT/` 目录存在
  - `nav.adoc` 定义导航结构

### Requirement: Runtime Verification Documentation
The library documentation SHALL include examples demonstrating runtime verification patterns.

#### Scenario: Network protocol verification
- **GIVEN** a user wants to verify network packet layouts at runtime
- **WHEN** they read the documentation
- **THEN** they find a complete example showing hash embedding and verification

#### Scenario: File format verification
- **GIVEN** a user wants to verify file data layouts at runtime
- **WHEN** they read the documentation
- **THEN** they find a complete example showing hash storage in file headers

### Requirement: Best Practices Guide
The library documentation SHALL include best practices for runtime verification scenarios.

#### Scenario: Hash storage guidance
- **GIVEN** a user needs to embed layout hashes in data
- **WHEN** they read the best practices guide
- **THEN** they understand recommended patterns for hash storage and comparison

### Requirement: Comprehensive Architecture Analysis Documentation

The library SHALL provide a comprehensive architecture analysis document that explains:
1. The core problem domain (binary compatibility challenges)
2. The design philosophy and solution approach
3. The two-layer architecture (Core vs Utility)
4. P2996 reflection mechanism internals
5. Signature generation algorithm details
6. Serialization safety checking mechanisms

#### Scenario: New user reads architecture analysis
- **WHEN** a developer accesses the architecture analysis document
- **THEN** they SHALL understand the library's purpose, design, and implementation approach

### Requirement: Competitive Comparison Documentation

The library SHALL provide documentation comparing it against alternative approaches:
1. Traditional methods (static_assert sizeof, #pragma pack)
2. Serialization frameworks (Protocol Buffers, FlatBuffers, Cap'n Proto)
3. Reflection libraries (magic_get, boost::pfr)
4. The library's unique value proposition

#### Scenario: Evaluator compares solutions
- **WHEN** a developer evaluates different binary compatibility solutions
- **THEN** they SHALL find clear comparison criteria and trade-off analysis

### Requirement: Performance Analysis Documentation

The library SHALL provide performance analysis documentation including:
1. Compile-time overhead analysis
2. Zero runtime overhead proof
3. Signature string length and hash collision analysis
4. Compilation time comparison with other methods

#### Scenario: Performance-conscious developer evaluates library
- **WHEN** a developer needs to assess performance implications
- **THEN** they SHALL find quantitative analysis and benchmarks

### Requirement: 变更日志

项目 SHALL 包含符合 Keep a Changelog 格式的 CHANGELOG.md 文件，记录所有版本的变更。

#### Scenario: 用户查看版本变更

- **WHEN** 用户打开 CHANGELOG.md
- **THEN** 显示所有版本的变更记录
- **AND** 每个版本包含 Added/Changed/Deprecated/Removed/Fixed/Security 分类
- **AND** 版本号遵循语义化版本控制 (SemVer)

### Requirement: 贡献指南

项目 SHALL 包含 CONTRIBUTING.md 文件，指导贡献者参与项目开发。

#### Scenario: 贡献者了解开发流程

- **WHEN** 贡献者打开 CONTRIBUTING.md
- **THEN** 显示开发环境设置说明
- **AND** 显示代码风格指南
- **AND** 显示 Pull Request 流程

#### Scenario: 用户报告问题

- **WHEN** 用户创建 Issue
- **THEN** 显示问题报告模板
- **AND** 模板包含重现步骤、期望行为、实际行为等字段

### Requirement: Technical Talk Materials

The project SHALL provide technical conference submission materials including a compelling abstract and presentation outline.

#### Scenario: Abstract attracts reviewer attention
- **WHEN** a conference reviewer reads the abstract
- **THEN** the abstract clearly communicates:
  - The novel application of P2996 static reflection
  - The real-world problem being solved (ABI compatibility)
  - The zero-runtime-cost benefit
  - Why this matters to practicing C++ developers

#### Scenario: Abstract meets format requirements
- **WHEN** submitting to a technical conference
- **THEN** the abstract is between 300-500 words
- **AND** includes title, key takeaways, and target audience

#### Scenario: Outline provides complete structure
- **WHEN** a reviewer evaluates presentation feasibility
- **THEN** the outline shows clear time allocation
- **AND** includes planned live demonstrations

### Requirement: Documentation Accuracy
README documentation SHALL accurately reflect the current API and capabilities.

#### Scenario: API Functions Listed
- **WHEN** reviewing the README API Reference
- **THEN** all 7 core functions are listed
- **AND** function descriptions match implementation

#### Scenario: Concepts Listed  
- **WHEN** reviewing the README Concepts section
- **THEN** all 5 concepts are listed with correct names
- **AND** deprecated concept names are removed

### Requirement: Documentation Conciseness
README SHALL be concise and focused on getting started quickly.

#### Scenario: README Length
- **WHEN** measuring README line count
- **THEN** it SHOULD be under 200 lines
- **AND** detailed information links to online documentation

### Requirement: Example Coverage Principle
The documentation SHALL maintain examples that cover all core value dimensions (V1/V2/V3).

#### Scenario: Minimum coverage per core value
- **GIVEN** the library has three core values V1 (Layout reliability), V2 (Definition precision), V3 (Projection)
- **THEN** the example suite SHALL include at least one type that demonstrates each value
- **AND** each value SHALL have both a "positive case" (value confirmed) and a "boundary case" (value limitation)

#### Scenario: V2 demonstration requirement
- **GIVEN** V2 distinguishes structural differences invisible to Layout
- **THEN** the examples SHALL include at least one pair of types where Layout matches but Definition differs
- **AND** the documentation SHALL explain why Definition is needed for that scenario

#### Scenario: Safety classification coverage
- **GIVEN** the safety classification system (Safe/Warning/Risk)
- **THEN** the examples SHALL include at least one type at each safety level
- **AND** the documentation SHALL explain why each type received its rating

### Requirement: IPC Application Scenario Documentation
The documentation SHALL include a dedicated section analyzing the shared memory / IPC application scenario.

#### Scenario: End-to-end IPC workflow
- **GIVEN** two processes sharing memory via mmap or POSIX shared memory
- **THEN** the documentation SHALL demonstrate the complete workflow from type definition through signature export to compile-time verification
- **AND** the documentation SHALL show that verified types can be used with zero-copy transfer (direct memcpy/mmap access)

#### Scenario: IPC comparison with alternatives
- **GIVEN** alternative approaches to cross-process data sharing (manual serialization, boost::interprocess, protobuf)
- **THEN** the documentation SHALL provide a comparison matrix covering: verification timing, runtime overhead, code complexity, and cross-platform safety
- **AND** the documentation SHALL position TypeLayout as complementary to serialization frameworks for dynamic-size data

### Requirement: Network Protocol Application Scenario Documentation
The documentation SHALL include a dedicated section analyzing the network protocol wire-format verification scenario.

#### Scenario: Wire-format verification workflow
- **GIVEN** a binary network protocol with fixed-size header structures
- **THEN** the documentation SHALL demonstrate how TypeLayout verifies wire-format consistency between client and server platforms
- **AND** the documentation SHALL contrast this with manual offsetof/sizeof assertions showing TypeLayout's automatic completeness

#### Scenario: Byte order boundary documentation
- **GIVEN** TypeLayout encodes endianness in the architecture prefix
- **THEN** the documentation SHALL explain that endianness mismatch is detected (signatures differ) but byte-order conversion is the user's responsibility
- **AND** the documentation SHALL describe this as intentional separation of concerns

### Requirement: File Format Application Scenario Documentation
The documentation SHALL include a dedicated section analyzing the file format compatibility scenario.

#### Scenario: Cross-platform file header verification
- **GIVEN** a binary file format with a fixed-size header structure
- **THEN** the documentation SHALL demonstrate how TypeLayout verifies that files written on one platform can be read on another
- **AND** the documentation SHALL show that Layout signature match guarantees safe fread/fwrite of the entire header

#### Scenario: Version evolution detection with Definition signatures
- **GIVEN** a file format header that evolves across versions (field renames, semantic changes)
- **THEN** the documentation SHALL demonstrate that Layout signatures may still match (same byte layout) while Definition signatures correctly detect structural changes
- **AND** the documentation SHALL present this as a key advantage of the two-layer system for file format maintenance

### Requirement: Plugin ABI Application Scenario Documentation
The documentation SHALL include a dedicated section analyzing the plugin system ABI compatibility scenario.

#### Scenario: Plugin load-time signature verification
- **GIVEN** a host application dynamically loading plugins via dlopen/LoadLibrary
- **THEN** the documentation SHALL demonstrate two verification modes:
  - Compile-time: TYPELAYOUT_ASSERT_COMPAT when host and plugin are built together
  - Runtime: Signature string comparison at plugin load time via dlsym
- **AND** the documentation SHALL explain that signature mismatch prevents undefined behavior from ABI incompatibility

#### Scenario: ODR violation detection
- **GIVEN** host and plugin may independently define structs with the same name but different layouts
- **THEN** the documentation SHALL demonstrate how Definition signatures detect ODR violations that compilers and linkers typically miss
- **AND** the documentation SHALL position this as a unique safety benefit for plugin architectures

### Requirement: Formal Proof Completeness

The PROOFS.md document SHALL provide formally complete proofs covering all three Core Values (V1 Layout Reliability, V2 Definition Precision, V3 Layer Projection), with every proof chain grounded in explicit grammar definitions and structural induction.

#### Scenario: V2 proof chain is complete
- **GIVEN** the formal proofs document (PROOFS.md)
- **WHEN** verifying V2 (Definition Precision) coverage
- **THEN** the document SHALL include:
  - Definition 1.9 (Definition Signature Grammar) with full BNF productions
  - Lemma 1.9.1 (Definition grammar unambiguity) with LL(1) argument
  - Theorem 3.5 (Definition Encoding Faithfulness) with decode_D construction
  - Corollary 3.5.1 (Definition Soundness) as direct consequence

#### Scenario: Primitive type verification is exhaustive
- **GIVEN** the §5.1 verification table in PROOFS.md
- **WHEN** comparing against the implementation's type specializations
- **THEN** the table SHALL cover all primitive type categories: fixed-width integers, floating point, platform-dependent integers, character types, boolean, byte, nullptr, pointers, references, member pointers, and function pointers

#### Scenario: Implementation mechanical details are documented
- **GIVEN** the signature generation uses a comma-prefix accumulation strategy
- **WHEN** reading the denotation definitions in §2
- **THEN** the document SHALL include an implementation note explaining that `skip_first()` is a purely mechanical string transformation equivalent to `join(",", fields)` and does not affect encoding faithfulness

### Requirement: Analysis Documentation
Analysis documents SHALL avoid content duplication. The `CROSS_PLATFORM_COLLECTION.md` document SHALL focus on collection-level compatibility matrix analysis and cross-reference `ZERO_SERIALIZATION_TRANSFER.md` for safety classification details, remediation patterns, and formal ZST theory. Example code in `example/compat_check.cpp` SHALL use concise comments that convey essential information without excessive verbosity.

#### Scenario: Cross-reference instead of duplication
- **WHEN** `CROSS_PLATFORM_COLLECTION.md` discusses safety classification
- **THEN** it provides a brief summary and links to `ZERO_SERIALIZATION_TRANSFER.md` for the full algorithm and formal justification

#### Scenario: Example code comments
- **WHEN** a developer reads `example/compat_check.cpp`
- **THEN** the header comment block is at most 10 lines and inline comments explain "why" not "what"

