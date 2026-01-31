## 1. 项目配置
- [x] 1.1 创建 Antora 站点配置文件 `doc/antora.yml`
- [x] 1.2 创建 Antora playbook 文件 `doc/antora-playbook.yml`
- [x] 1.3 创建文档目录结构

## 2. 主页和概述
- [x] 2.1 创建文档主页 `modules/ROOT/pages/index.adoc`
- [x] 2.2 创建库概述页 `modules/ROOT/pages/overview.adoc`
- [x] 2.3 创建快速入门页 `modules/ROOT/pages/quickstart.adoc`
- [x] 2.4 创建导航配置 `modules/ROOT/nav.adoc`

## 3. 用户指南
- [x] 3.1 创建用户指南索引 `user-guide/index.adoc`
- [x] 3.2 创建布局签名指南 `user-guide/layout-signatures.adoc`
- [x] 3.3 创建类型支持指南 `user-guide/type-support.adoc`
- [x] 3.4 创建可移植性检查指南 `user-guide/portability.adoc`
- [x] 3.5 创建模板约束指南 `user-guide/concepts.adoc`
- [x] 3.6 创建哈希验证指南 `user-guide/hash-verification.adoc`
- [x] 3.7 创建继承支持指南 `user-guide/inheritance.adoc`
- [x] 3.8 创建位域支持指南 `user-guide/bitfields.adoc`

## 4. API 参考
- [x] 4.1 创建 API 参考索引 `reference/index.adoc`
- [x] 4.2 创建核心函数参考 `reference/core-functions.adoc`
- [x] 4.3 创建 Concepts 参考 `reference/concepts.adoc`
- [x] 4.4 创建宏参考 `reference/macros.adoc`
- [x] 4.5 创建工具类参考 `reference/utility-classes.adoc`
- [x] 4.6 创建类型签名参考 `reference/type-signatures.adoc`

## 5. 设计原理
- [x] 5.1 创建设计原理索引 `design/index.adoc`
- [x] 5.2 创建签名格式设计 `design/signature-format.adoc`
- [x] 5.3 创建哈希算法选择 `design/hash-algorithms.adoc`
- [x] 5.4 创建可移植性策略 `design/portability-strategy.adoc`
- [x] 5.5 创建编译时计算设计 `design/compile-time.adoc`
- [x] 5.6 创建 P2996 反射使用 `design/reflection-usage.adoc`

## 6. 配置选项
- [x] 6.1 创建配置选项页 `config/index.adoc`
- [x] 6.2 创建编译器选项说明 `config/compiler-options.adoc`
- [x] 6.3 创建平台支持说明 `config/platform-support.adoc`

## 7. 示例代码
- [x] 7.1 创建示例索引 `examples/index.adoc`
- [x] 7.2 创建基本使用示例 `examples/basic-usage.adoc`
- [x] 7.3 创建网络协议示例 `examples/network-protocol.adoc`
- [x] 7.4 创建共享内存示例 `examples/shared-memory.adoc`
- [x] 7.5 创建序列化示例 `examples/serialization.adoc`

## 8. 构建和验证
- [x] 8.1 创建文档构建脚本 `doc/build-docs.sh` 和 `doc/build-docs.cmd`
- [x] 8.2 验证 Antora 构建成功 ✅ 31 个 HTML 页面无警告生成
- [x] 8.3 验证所有示例代码可编译 ✅ 在 WSL P2996 环境验证通过
- [x] 8.4 清理旧的 Markdown 文档（保留作为备份）

## 9. 收尾工作
- [x] 9.1 更新 README.md 文档链接
- [ ] 9.2 添加文档贡献指南 (可选)
- [x] 9.3 验证所有内部链接有效 ✅ xref 路径已修复，构建无错误
