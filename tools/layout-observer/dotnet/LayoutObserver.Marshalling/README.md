# Runtime-marshalling profiles

此后端只测显式注册的 **CoreCLR 10 runtime-marshalling** 表示。`Marshal.SizeOf/OffsetOf` 不代表托管布局；NativeAOT、Mono、源生成 P/Invoke 和任意自定义封送器需要各自适配器。

从仓库根目录、固定 SDK 已在 PATH 时运行：

```powershell
dotnet run --project tools/layout-observer/dotnet/LayoutObserver.Marshalling -c Release -- --configuration Release --output marshaled.json --native-library build-observer-marshal/Release/typelayout_marshal.dll
```

原生库按 [native/marshalling](../../native/marshalling/README.md) 构建。`--native-library` 会额外核对真实 C++ `sizeof/offsetof`，并通过 P/Invoke 检查所有字段的哨兵值和错误输入；跨平台使用实际平台动态库。省略该选项可单独采集已注册 profile，但不等于跑过原生 oracle。

| Profile | 托管声明 | 封送表示 |
| --- | --- | --- |
| marshal-default | bool、char、int | 默认 Bool 的 4 字节整数、Unicode char 的 2 字节 code unit、int32 |
| marshal-byte | `[MarshalAs(U1)] bool`、char、int | 1 字节整数、Unicode char、int32 |
| marshal-array | `[ByValArray(SizeConst=3, ArraySubType=I4)] int[]`、short | 3 个内联 int32 元素和 int16；保留数组/元素 observation |
| marshal-custom | 未注册的自定义封送路径 | unsupported；要求适配器与实例契约 |

快照保留 profile ID、mechanism、配置、字段实际封送偏移和类型表示。bool 的字段描述是指定 wire storage（i32/u8）；并不改变 C# bool 本身的表示。原生 fixture 仅约定这里的类型与调用路径，不把所有非零整数或任意调用 ABI 视为已验证。数组的元素位置来自显式连续 I4 profile，并由真实 P/Invoke 接收方验证。

用 `cases/marshaled.compare.json` 比较原生快照与封送快照。把托管视图误选到 marshaled-layout 模式会拒绝；regression 的每一级 observation 都要求相同 profile。对齐未测得时保留 unknown，自定义封送器不会落入默认 profile。

依据：[Microsoft Learn — 自定义结构封送](https://learn.microsoft.com/en-us/dotnet/standard/native-interop/customize-struct-marshalling)，核对日期 2026-09-28。新增 profile 需要同步字段存储规则、独立原生 fixture 与 P/Invoke 哨兵值检查。
