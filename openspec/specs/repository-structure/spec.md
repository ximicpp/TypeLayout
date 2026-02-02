# repository-structure Specification

## Purpose
TBD - created by archiving change simplify-repository. Update Purpose after archive.
## Requirements
### Requirement: 精简仓库结构

TypeLayout 仓库 SHALL 移除所有废弃的兼容性头文件，保持清晰的两层架构。

#### Scenario: 废弃文件已移除
- **GIVEN** 仓库包含 detail/ 目录和根目录废弃文件
- **WHEN** 执行精简操作
- **THEN** 以下目录/文件被移除：
  - `detail/` 目录 (7个文件)
  - `signature.hpp`, `verification.hpp`, `concepts.hpp`, `portability.hpp`

#### Scenario: 核心结构保持完整
- **GIVEN** 精简后的仓库
- **WHEN** 检查目录结构
- **THEN** 应包含：
  - `core/` 目录 (8个核心文件)
  - `util/` 目录 (3个工具文件)
  - `typelayout.hpp`, `typelayout_util.hpp`, `typelayout_all.hpp`
  - `fwd.hpp`, `compat.hpp`

#### Scenario: 所有测试通过
- **GIVEN** 精简后的仓库
- **WHEN** 运行测试套件
- **THEN** 所有测试应通过

