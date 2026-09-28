# Layout Observer

跨语言、跨构建的内存布局观测与比较工具。输入实际采集的 C++ / C# 快照和显式类型、字段映射，输出逐字段 JSON diff、终端结论和离线 HTML。

它独立于 TypeLayout 的 header-only 签名库，原有构建、签名和 byte-copy admission 不变。`same` 仅表示在报告所列 scope/policy 下已知事实一致，不授予调用 ABI、序列化、所有权或任意内存复制的兼容保证。

当前实现与真实验证环境见 [STATUS.md](STATUS.md)，总体设计见 [设计文档](../../docs/design/cross-language-layout.md)，机器契约见 [PROTOCOL.md](contracts/PROTOCOL.md)。

## 快速使用

需要 .NET SDK **10.0.401**；C++ 采集另外需要对应后端工具链。先进入本目录，让 `global.json` 生效：

```sh
cd tools/layout-observer
dotnet --version
dotnet build dotnet/LayoutObserver.Cli -c Release
dotnet run --project dotnet/LayoutObserver.Managed -c Release -- --configuration Release --output artifacts/managed-release.json
dotnet run --project dotnet/LayoutObserver.Cli -c Release -- validate --input artifacts/managed-release.json
```

每次使用新的输出文件名，工具不会覆盖快照、报告或基准。CLI 的 `compare` 只读输入；下面的文件名代表你已采集的实际快照：

```sh
dotnet run --project dotnet/LayoutObserver.Cli -c Release -- compare \
  --left artifacts/native-release.json --right artifacts/managed-release.json \
  --manifest cases/shared.compare.json \
  --out artifacts/comparison.json --html artifacts/comparison.html
```

Windows PowerShell 可把上述多行命令写成一行。直接离线打开 HTML，即可并排查看字段、嵌套、重叠、运行时区域、证据和未知项。

| 退出码 | 含义 |
| --- | --- |
| 0 | 全部请求在指定 policy 下相同且覆盖完整 |
| 1 | 有已确认差异，所有必需信息完整 |
| 2 | 不完整、不可比较或必需能力未支持；已知差异仍保留 |
| 3 | 输入、构建、运行、符号或采集错误 |

默认 `value-fields-v1` 检查值大小、字段偏移/表示/占用、嵌套与相关运行时区域；`value-alignment-v1` 额外要求对齐。C# 未测得对齐时，严格策略返回 2。`scope=array` 还要求根 observation 的数组步长；`scope=object` 要求已校准的对象原点和运行时对象尺寸。

## 采集后端

| 后端 | 入口 | 用途 |
| --- | --- | --- |
| C++ P2996 | [native](native/README.md) | 实际 `sizeof/alignof`、字段/基类/位域反射、嵌套树 |
| MSVC DIA | [native/msvc](native/msvc/README.md) | 实际 PE/PDB，校验 RSDS GUID/age，再读取私有字段与子对象 |
| C# 静态探针 | [Managed](dotnet/LayoutObserver.Managed/README.md) | typed-byref、静态生成访问；CoreCLR / NativeAOT |
| CoreCLR 对象 | [ClrMd](dotnet/LayoutObserver.ClrMd/README.md) | 受控工厂、压缩 GC 屏障、冻结快照、逻辑 ID 重定位 |
| runtime-marshalling | [Marshalling](dotnet/LayoutObserver.Marshalling/README.md) | 显式封送 profile；与真实原生 P/Invoke 接收端核对 |

新增业务类型使用后端注册/工厂接口，再写显式字段映射；不能凭名称猜测跨语言类型相等。任意业务进程、任意封送器、未验证运行时并非自动支持。未支持的请求保留诊断，不从批次中删掉。

## 一次运行多个构建

`run` 接受 JSON manifest，构建、采集、校验实际架构/配置/能力，写入源码与产物摘要，再逐项比较。命令使用参数数组，无 shell 展开。整个进程树与输出收尾共享超时；Windows Job / Linux 进程组负责清理工具启动的后代。

首个必需矩阵是 **Linux x64 × C++/C# × Debug/Release**。在真正支持当前 P2996 API 的编译器环境中：

```sh
python3 scripts/run-value-matrix.py --compiler clang++ --output artifacts/linux-values-001
```

脚本保留生成的 run manifest、四组独立构建、stdout/stderr、原始/补充环境信息的快照、四份 diff/HTML 和 `run.json`。可用 `--dotnet`、`--cmake` 和 `--linker-flags` 指定局部工具链。能力探针失败、缺 runner、架构错误都会失败，不会替换工具链或忽略矩阵单元。

其他矩阵使用 [run-manifest schema](contracts/run-manifest.schema.json) 配置：每个 profile 声明 executable、arguments、workingDirectory、artifact、实际目标、配置、能力和 buildSteps；每个 comparison 引用两个 profile 和一个 compare manifest。运行：

```sh
dotnet run --project dotnet/LayoutObserver.Cli -c Release -- run --manifest my-matrix.json --out-dir artifacts/run-002
```

生成目录应位于 Git 忽略目录或仓库外。源码在整个运行期间必须保持稳定。对已有产物只计算 hash 不会证明其来自当前源码；需要记录实际 buildSteps。原始 collector 的来源声明会保留，未知来源不会被标为 clean。

## 验证

```sh
dotnet run --project dotnet/LayoutObserver.CoreChecks -c Release
dotnet run --project dotnet/LayoutObserver.ReportChecks -c Release
dotnet run --project dotnet/LayoutObserver.ProcessChecks -c Release
dotnet run --project dotnet/LayoutObserver.ManagedChecks -c Release
dotnet run --project dotnet/LayoutObserver.ClrMdChecks -c Release -- dotnet/LayoutObserver.ObjectHost/bin/Release/net10.0/LayoutObserver.ObjectHost.dll
```

先构建对应项目；ClrMdChecks 需要同配置的 ObjectHost，具体命令见其 README。原生后端分别运行 CTest，完整 CLI 集成使用 `scripts/check-integration.py --help`。`.NET` 第三方依赖在 Generator/ClrMd 的 `packages.lock.json` 中锁定，重现时使用 `dotnet restore --locked-mode`；AOT 工具包由 SDK/RID/运行时版本确定。

正式 JSON Schema 验证需要 Python 与 `scripts/requirements-validation.txt`；运行 `scripts/check-contracts.py snapshot FILE...` 或对应 manifest/diff 类型。Core 还检查唯一 ID、图关系、范围、覆盖率等语义；JSON Schema 通过不能替代这些检查。原有库的 CMake/CTest 保持独立执行。
