# Change: Analyze Application — File Format Cross-Version/Cross-Platform Compatibility

## Why
文件格式 (如自定义数据文件、配置文件、日志文件) 需要确保 reader 和 writer 对二进制头部结构的
理解一致。TypeLayout 可以在编译时证明文件格式头的跨平台/跨版本兼容性。

## What Changes
- 分析文件格式场景的典型痛点 (版本漂移、平台差异)
- 展示 TypeLayout 验证文件头结构一致性的工作流
- 对比传统方案 (手动版本号检查、magic number、格式规范文档)
- 分析版本演进的处理方式 (V2 Definition 签名检测结构变更)
- 识别局限 (变长记录、压缩格式、嵌套结构)

## Impact
- Affected specs: documentation
- Affected code: 无代码变更，纯分析性质
