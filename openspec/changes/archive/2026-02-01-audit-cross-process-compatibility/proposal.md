# Change: 跨进程/跨机器类型兼容性检查功能审计

## Why

TypeLayout 的核心价值是确保**不同进程、不同机器上同一类型的二进制兼容性**。需要系统性审查当前实现是否正确和完备地覆盖所有可能导致不兼容的因素。

关键场景：
- 进程 A (Linux x64) 和进程 B (Linux x64) 共享内存
- 服务器 (Linux x64) 和客户端 (Windows x64) 网络通信
- 旧版本程序和新版本程序读取同一文件

## What Changes

### 审计维度

1. **架构差异因素** (Architecture)
   - [ ] 指针大小 (32-bit vs 64-bit)
   - [ ] 字节序 (Little Endian vs Big Endian)
   - [ ] 数据模型 (ILP32, LP64, LLP64)

2. **ABI 差异因素** (ABI)
   - [ ] 对齐规则差异
   - [ ] 结构体填充 (padding) 差异
   - [ ] `long` 类型大小 (4 bytes on Windows, 8 bytes on Linux)
   - [ ] `wchar_t` 大小 (2 bytes on Windows, 4 bytes on Linux)
   - [ ] `long double` 大小 (8/12/16 bytes)
   - [ ] 枚举底层类型
   - [ ] 位域布局差异

3. **编译器差异因素** (Compiler)
   - [ ] 不同编译器的默认对齐
   - [ ] `#pragma pack` 影响
   - [ ] `alignas` 处理
   - [ ] 空基类优化 (EBO)
   - [ ] 虚表指针布局

4. **类型定义差异因素** (Type Definition)
   - [ ] 字段顺序变化
   - [ ] 字段类型变化
   - [ ] 字段增删
   - [ ] 嵌套类型变化

5. **浮点表示** (Floating Point)
   - [ ] IEEE 754 vs 其他格式
   - [ ] `long double` 实现差异

### 验证清单

- [ ] 签名格式是否完整包含所有兼容性因素？
- [ ] Hash 冲突概率是否足够低？
- [ ] 当前 arch prefix `[64-le]` 是否足够？需要更多信息？
- [ ] `Portable<T>` concept 是否正确排除了所有不可移植类型？
- [ ] 是否需要版本号或时间戳？
- [ ] 是否需要编译器标识？

## Impact

- Affected specs: `use-cases`, `analysis`
- Affected code: 
  - `signature.hpp` - arch prefix 可能需要扩展
  - `concepts.hpp` - Portable 定义可能需要细化
  - `type_signature.hpp` - 类型签名可能需要更多信息
