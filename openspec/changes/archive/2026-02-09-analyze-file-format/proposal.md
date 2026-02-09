# Change: 文件格式兼容场景与两层签名的关系分析

## Why
二进制文件格式（如日志、数据库、传感器记录）需要在不同平台和不同版本之间保持可读性。文件格式场景是两层签名系统的经典双层应用：Layout 层验证字节兼容性（能否安全 fread/fwrite），Definition 层检测版本演化（字段重命名、语义变更）。这是展示两层互补价值的最佳场景。

## What Changes
- 新增文件格式兼容场景的深度分析文档
- 分析内容包括：
  - Layout 层保证跨平台 fread/fwrite 安全的形式化链
  - Definition 层检测版本演化的独特价值
  - 两层互补的典型案例（Layout 匹配但 Definition 不匹配）
  - 与 Protocol Buffers / FlatBuffers 等序列化方案的定位对比

## Impact
- Affected specs: signature
- Affected code: 无代码变更
