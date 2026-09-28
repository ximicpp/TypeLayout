# Native runtime-marshalling fixture

独立 C++20 动态库和原生布局快照，不需要 P2996、DIA 或旧 TypeLayout admission。它为指定的 C# runtime-marshalling fixture 提供真实 P/Invoke 接收端。

从仓库根目录构建 Windows x64：

```powershell
cmake -S tools/layout-observer/native/marshalling -B build-observer-marshal -G "Visual Studio 17 2022" -A x64
cmake --build build-observer-marshal --config Release --parallel
ctest --test-dir build-observer-marshal -C Release --output-on-failure
build-observer-marshal/Release/typelayout-marshal-native.exe --configuration Release --run-id native-marshal --output native-marshal.json
```

库为 `typelayout_marshal.dll`。Linux 常规 C++20 工具链可用 `-DCMAKE_BUILD_TYPE=Release` 构建单配置目录，库为 `libtypelayout_marshal.so`。这是当前源码支持的构建路径；已验证平台以任务实际测试记录为准。

`--output` 通过操作系统独占创建文件；路径已存在时退出 3，原文件内容保持不变。每次采集需使用新的输出路径。未嵌入源码身份时 `sourceDirty` 为 `null`，由构建调度器补全后才能接纳为可复现 baseline。

| caseId | 原生类型 | 字段编号 | 默认布局大小 |
| --- | --- | --- | --- |
| 0 | `DefaultBoolChar` | 0 enabled/int32、1 letter/char16、2 count/int32 | 12 |
| 1 | `ByteBoolChar` | 0 enabled/uint8、1 letter/char16、2 count/int32 | 8 |
| 2 | `InlineInts` | 0 values/int32[3]、1 code/int16 | 16 |

导出为 C linkage/cdecl，函数声明见 [fixtures.hpp](fixtures.hpp)：

- `typelayout_marshal_size(caseId)` 和 `typelayout_marshal_offset(caseId, fieldId)` 返回 uint64；非法编号返回 `UINT64_MAX`。
- `typelayout_marshal_validate_default(ptr)` / `_byte(ptr)` 要求 `enabled=1, letter=0x4E2D, count=0x12345678`。
- `typelayout_marshal_validate_array(ptr)` 要求 `values=[0x11223344,-7,42], code=-1234`。
- validate 接收地址为只读，全部正确返回 1；nullptr 或任一值不符返回 0。

快照根 ID 为 `marshal-default`、`marshal-byte`、`marshal-array`；字段与数组元素保留完整 tree。enabled 在原生表示里是 i32/u8，C# 封送快照应描述指定 profile 的实际整数存储，不能把托管 bool 的一字节表示混入。char16 明确 utf16 code unit，signedness 不适用。

动态库的 oracle 验证真实尺寸/偏移、所有 sentinel、nullptr 和不匹配数据。快照另测确定的 sizeof/offsetof、数组关系和 Unicode 输出路径；重复输出到相同路径时验证退出 3 且原文件 SHA-256 不变。跨语言验收还必须由 C# 实际 P/Invoke 调用完成；这组 fixture 通过只支持这些类型与这个封送路径，不能推导一般调用 ABI 兼容。
