## MODIFIED Requirements

### Requirement: 简化目录结构
仓库 SHALL 采用最小化单层结构，只保留核心签名功能。

#### Scenario: 最小化结构
- **GIVEN** 精简后的仓库
- **WHEN** 检查目录结构
- **THEN** 应只包含：
  - `core/` 目录 (8 个核心文件)
  - `typelayout.hpp` (唯一入口头文件)

#### Scenario: 无工具层
- **GIVEN** 精简后的仓库
- **WHEN** 检查 `include/boost/typelayout/` 目录
- **THEN** 不应存在：
  - `util/` 目录
  - `typelayout_util.hpp`
  - `typelayout_all.hpp`
  - `compat.hpp`
  - `fwd.hpp`

## REMOVED Requirements

### Requirement: 工具层
**Reason**: 工具层功能可由用户基于核心 API 自行实现，不属于库的核心价值。
**Migration**: 用户如需序列化检查功能，可参考旧版 `util/serialization_check.hpp` 自行实现。

### Requirement: 双入口头文件
**Reason**: 简化 API，只保留单一入口 `typelayout.hpp`。
**Migration**: 将 `#include <boost/typelayout/typelayout_all.hpp>` 改为 `#include <boost/typelayout/typelayout.hpp>`。
