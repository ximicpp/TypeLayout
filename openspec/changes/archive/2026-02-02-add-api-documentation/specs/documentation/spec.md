## MODIFIED Requirements

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
