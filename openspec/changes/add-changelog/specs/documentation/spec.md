## ADDED Requirements

### Requirement: 变更日志

项目 SHALL 包含符合 Keep a Changelog 格式的 CHANGELOG.md 文件，记录所有版本的变更。

#### Scenario: 用户查看版本变更

- **WHEN** 用户打开 CHANGELOG.md
- **THEN** 显示所有版本的变更记录
- **AND** 每个版本包含 Added/Changed/Deprecated/Removed/Fixed/Security 分类
- **AND** 版本号遵循语义化版本控制 (SemVer)
