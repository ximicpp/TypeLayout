## 1. 场景分析
- [x] 1.1 描述 wire-format 验证的典型痛点
- [x] 1.2 展示 TypeLayout 的协议头验证工作流
- [x] 1.3 对比手动 offsetof 方式的缺陷

## 2. 竞品对比
- [x] 2.1 对比 protobuf, FlatBuffers, Cap'n Proto
- [x] 2.2 分析"wire-format 控制权"的关键差异
- [x] 2.3 分析跨语言的限制

## 3. 边界分析
- [x] 3.1 分析字节序检测 vs 转换的职责边界
- [x] 3.2 分析不同协议类型的适用性矩阵
- [x] 3.3 识别变长字段 / TLV 编码的限制

## 4. 文档记录
- [x] 4.1 将网络协议场景分析记录到 documentation spec
