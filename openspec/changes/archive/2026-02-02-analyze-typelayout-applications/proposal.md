# Change: 分析 TypeLayout 核心应用场景

## Why
TypeLayout 的核心功能（布局签名、哈希、序列化检查）已经完整。
需要系统分析这些核心功能如何支撑各种重大应用场景，明确库的核心价值定位。

## What Changes
本次为分析工作，产出应用场景分析文档，包括：
- 每个应用场景的详细分析
- TypeLayout 核心功能如何支撑该场景
- 示例代码和最佳实践
- 库的核心价值总结

### 待分析的应用场景
1. **零拷贝 IPC** - 进程间共享内存通信
2. **二进制文件格式验证** - 存档/配置文件兼容性
3. **网络协议类型安全** - 客户端/服务端一致性
4. **内存映射数据库** - Schema 兼容性保护
5. **插件系统 ABI 兼容** - 动态库安全加载

### TypeLayout 核心功能
- `get_layout_signature<T>()` - 完整类型布局描述
- `get_layout_hash<T>()` - 快速比较哈希
- `is_serializable_v<T>` - 编译时安全检查
- `PlatformSet` - 跨平台兼容约束

## Impact
- 本次为纯分析工作
- 产出: `specs/application-analysis/spec.md`
- 可用于指导文档编写和示例开发
