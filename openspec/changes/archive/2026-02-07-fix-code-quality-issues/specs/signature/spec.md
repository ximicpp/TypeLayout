## MODIFIED Requirements

### Requirement: Type Signature Correctness
类型签名生成 SHALL 在所有支持的平台上正确编译，不产生类型特化冲突。

#### Scenario: 平台类型特化无冲突
- **WHEN** 在 Windows/Linux/macOS 上编译
- **THEN** 所有基本类型（`signed char`、`long`、`long long` 等）均有唯一特化
- **AND** 不出现 "redefinition" 编译错误

### Requirement: CompileString Safety
编译期字符串比较 SHALL 不产生越界访问。

#### Scenario: 字符串比较边界安全
- **WHEN** 比较两个 `CompileString` 对象
- **THEN** 不访问超出缓冲区范围的内存
- **AND** 正确处理不同大小缓冲区的比较

## ADDED Requirements

### Requirement: Endianness Detection Robustness
端序检测 SHALL 使用 C++20 `std::endian` 作为首选方法。

#### Scenario: 大端平台检测正确
- **WHEN** 在大端平台上编译
- **THEN** `TYPELAYOUT_LITTLE_ENDIAN` 为 `false`
- **AND** 签名前缀正确反映 `-be` 端序

### Requirement: C++20 Version Check
库 SHALL 在编译时检查 C++ 版本，要求至少 C++20。

#### Scenario: 旧版本编译器报错
- **WHEN** 使用 C++17 或更早版本编译
- **THEN** 产生清晰的编译错误信息
