# limitations Specification

## Purpose
TBD - created by archiving change analyze-remaining-limitations. Update Purpose after archive.
## Requirements
### Requirement: 签名驱动的兼容性模型

TypeLayout 的序列化兼容性 SHALL 完全基于签名比较，不区分"同编译器"或"跨编译器"场景。

#### Scenario: 签名匹配即兼容
- **GIVEN** 两个端点（序列化端和反序列化端）
- **WHEN** 两端对同一类型生成的签名相同
- **THEN** 类型 SHALL 被视为兼容
- **AND** memcpy 序列化 SHALL 是安全的

#### Scenario: 签名不匹配即不兼容
- **GIVEN** 两个端点
- **WHEN** 两端对同一类型生成的签名不同
- **THEN** 类型 SHALL 被视为不兼容
- **AND** 序列化检查 SHALL 失败

#### Scenario: 签名包含完整布局信息
- **GIVEN** 任意类型 T
- **WHEN** 生成布局签名
- **THEN** 签名 SHALL 包含足够信息以确定内存布局：
  - 成员偏移量（包括虚拟基类偏移）
  - 成员大小和对齐
  - 位域位置和宽度

---

### Requirement: 虚拟继承完全兼容

虚拟继承类型 SHALL 被视为 TypeLayout 完全兼容，因为签名中已包含偏移信息。

#### Scenario: 虚拟基类签名包含偏移
- **GIVEN** 一个包含虚拟继承的类型
- **WHEN** 生成布局签名
- **THEN** 签名 SHALL 包含虚拟基类的实际偏移量
- **AND** 格式为 `@offset[vbase]:class{...}`

#### Scenario: 虚拟继承跨平台检测
- **GIVEN** 相同源码在不同编译器上编译
- **WHEN** 虚拟基类偏移不同（因 ABI 差异）
- **THEN** 生成的签名 SHALL 不同
- **AND** 签名比较 SHALL 检测到不兼容
- **EXAMPLE** GCC: `@8[vbase]:A{...}` vs MSVC: `@12[vbase]:A{...}` → 不匹配

#### Scenario: 虚拟继承序列化检查
- **GIVEN** 一个包含虚拟基类的类型
- **WHEN** 检查 `is_serializable_v<T>`
- **THEN** 结果 SHALL 基于签名是否可比较（不因虚拟继承而拒绝）
- **AND** 虚拟继承本身不是序列化阻止因素

---

### Requirement: 标准库类型兼容性策略

TypeLayout SHALL 定义明确的标准库类型支持策略，区分"专用特化"、"结构体反射"和"不支持"三种处理方式。

#### Scenario: std::tuple 处理策略
- **GIVEN** 用户请求 `std::tuple<T...>` 的布局签名
- **WHEN** 类型被反射
- **THEN** 实现 SHALL 使用通用结构体反射
- **AND** 签名 SHALL 反映实际的内存布局（可能包含 EBO 优化）

#### Scenario: std::variant 处理策略
- **GIVEN** 用户请求 `std::variant<T...>` 的布局签名
- **WHEN** 类型被反射
- **THEN** 签名 SHALL 包含内部标签和联合体结构
- **AND** `is_serializable_v` SHALL 为 `false`（因为活动索引是运行时状态）

#### Scenario: std::optional 处理策略
- **GIVEN** 用户请求 `std::optional<T>` 的布局签名
- **WHEN** 类型被反射
- **THEN** 签名 SHALL 反映内部 engaged 标志和值存储
- **AND** `is_serializable_v` SHALL 为 `false`（因为 engaged 状态是运行时状态）

### Requirement: 位域兼容性

位域类型 SHALL 遵循签名驱动的兼容性模型，签名中包含位域位置信息。

#### Scenario: 位域签名包含位位置
- **GIVEN** 一个包含位域成员的结构体
- **WHEN** 生成布局签名
- **THEN** 签名 SHALL 包含位域的位位置和宽度
- **AND** 不同编译器的位域布局差异 SHALL 反映在签名中

#### Scenario: 位域序列化检查
- **GIVEN** 一个包含位域的结构体
- **WHEN** 检查 `is_serializable_v<T>`
- **THEN** 结果 SHALL 基于签名是否可比较（不因位域而拒绝）
- **AND** 位域本身不是序列化阻止因素

#### Scenario: 位域文档警告
- **GIVEN** 用户阅读位域文档
- **THEN** 文档 SHALL 说明：
  - 签名准确反映当前编译器的位域布局
  - 签名比较会自动检测不同编译器的布局差异
  - 建议跨平台协议使用显式位操作

---

