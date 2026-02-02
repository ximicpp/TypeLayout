## ADDED Requirements

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
