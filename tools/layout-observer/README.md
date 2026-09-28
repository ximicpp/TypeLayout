# Layout Compare

跨语言、跨构建的内存布局比较工具。将不同语言实现、源码版本、构建配置和运行环境的实际快照绑定到同一组逻辑类型，输出字段差异、覆盖率和独立的环境变化。目前提供 C++ / C# 采集器；比较内核通过公开协议接收其他采集器的数据。

它独立于 TypeLayout 的 header-only 签名库，原有构建、签名和 byte-copy admission 不变。`same` 仅表示在报告所列 scope/policy 下已知事实一致，不授予调用 ABI、序列化、所有权或任意内存复制的兼容保证。

当前实现与真实验证环境见 [STATUS.md](STATUS.md)，比较项目设计见 [设计文档](../../docs/design/layout-compare-project.md)，机器契约见 [PROTOCOL.md](contracts/PROTOCOL.md) 和 [签名协议](contracts/SIGNATURE.md)。目录与程序集仍保留 LayoutObserver 名称。

## 通过协议接入与独立签名

新增语言适配器只需输出公开快照协议并通过其声明能力的采集验证；公共内核完成语义校验、逻辑映射、签名生成与比较。无需改动比较器或报告。签名使用版本化结构化内容，完整时附带 SHA-256；摘要不能绕过完整性、原点、视图和封送规则检查。设计边界见[协议设计](../../docs/design/layout-signature-protocol.md)。

不同构建可在各自流水线独立生成签名。单侧 manifest 指定 `scope`、`policy` 与逻辑 case/字段映射，稍后按逻辑 case ID 配对。以订单示例为例，在本目录运行：

```sh
dotnet run --project dotnet/LayoutObserver.Cli -c Release -- signature \
  --input artifacts/orders-001/report/snapshots/native-release.json \
  --manifest examples/orders/native.signature-manifest.json --out artifacts/native.signature.json
dotnet run --project dotnet/LayoutObserver.Cli -c Release -- signature \
  --input artifacts/orders-001/report/snapshots/managed-release.json \
  --manifest examples/orders/managed.signature-manifest.json --out artifacts/managed.signature.json
dotnet run --project dotnet/LayoutObserver.Cli -c Release -- compare-signatures \
  --left artifacts/native.signature.json --right artifacts/managed.signature.json \
  --mode representation --out artifacts/signature.diff.json
```

生成退出 0 表示选定规则下信息完整，2 表示部分信息未知，3 表示输入或配置错误；比较仍使用下表退出码。未知观察保留可解释内容，不能通过相同的 unknown 摘要变成 `same`。`validate-signature --input FILE` 校验签名内容与摘要，其成功不代表布局相等。所有输出均拒绝覆盖。

共同映射必须使用同一逻辑绑定方式；显式映射字段与自动按原 ID 匹配的字段是不同命名空间，避免自动字段覆盖显式逻辑字段。签名是选定 scope/policy 的布局表示，不能与旧 `get_layout_signature<T>()` 字符串混用。

## 比较自己的类型与多个构建

[订单示例](examples/orders/README.md) 提供两个独立 consumer 项目、复用的类型/字段映射和五组真实构建。它展示 C++ / C# Debug/Release 的一致布局，以及修改 packing 后的确定差异。在已配置 P2996 的 Linux x64 环境中，从本目录执行：

```sh
python3 examples/orders/run.py --compiler clang++ --output artifacts/orders-001
```

如果快照已来自各自的构建流水线，直接编写 [project manifest](examples/orders/project.json)，运行：

```sh
dotnet run --project dotnet/LayoutObserver.Cli -c Release -- project \
  --manifest my-project.json --out-dir artifacts/comparison-001
```

项目的 `variants` 引用各构建快照，`mappings` 描述逻辑类型/字段到实际 observation/member 的映射，`comparisons` 明确指定比较组合、case 子集和策略。多个构建可复用一份 mapping；缺少输入或映射会失败。结果目录含 index.html、result.json、逐对 JSON/HTML，以及导入完整时可离线重放的 project.json。比较命令不需要构建器或原始代码。

每条报告按显式 case 配对布局，不依赖快照数组顺序。diff.context 单独记录语言、编译器、运行时、配置和来源变化；这些变化不自动改变布局结论，也不证明差异由某个编译选项导致。

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

其他矩阵使用 [run-manifest schema](contracts/run-manifest.schema.json) 配置：每个 profile 声明 executable、arguments、workingDirectory、artifact、实际目标、配置、能力和 buildSteps，可通过自己的 sourceRoot 绑定独立仓库/版本；每个 comparison 引用两个 profile 和一个 compare manifest。运行：

```sh
dotnet run --project dotnet/LayoutObserver.Cli -c Release -- run --manifest my-matrix.json --out-dir artifacts/run-002
```

生成目录应位于 Git 忽略目录或仓库外。源码在整个运行期间必须保持稳定，摘要覆盖 Git HEAD、index、工作树文件和递归子模块；未初始化子模块会失败。产物摘要在采集前后核对。原始 build 保存在 collectorProvenance，captureProvenance 记录核验范围。built-in-run 只说明执行了声明的 buildSteps；已有产物标为 unverified，hash 不证明源码与产物对应，仓库外依赖也需另行固定。

## 验证

```sh
dotnet run --project dotnet/LayoutObserver.CoreChecks -c Release
dotnet run --project dotnet/LayoutObserver.SignatureChecks -c Release
dotnet run --project dotnet/LayoutObserver.ReportChecks -c Release
dotnet run --project dotnet/LayoutObserver.ProcessChecks -c Release
dotnet run --project dotnet/LayoutObserver.ManagedChecks -c Release
dotnet run --project dotnet/LayoutObserver.ClrMdChecks -c Release -- dotnet/LayoutObserver.ObjectHost/bin/Release/net10.0/LayoutObserver.ObjectHost.dll
```

先构建对应项目；ClrMdChecks 需要同配置的 ObjectHost，具体命令见其 README。原生后端分别运行 CTest，完整 CLI 集成使用 `scripts/check-integration.py --help`。`.NET` 第三方依赖在 Generator/ClrMd 的 `packages.lock.json` 中锁定，重现时使用 `dotnet restore --locked-mode`；AOT 工具包由 SDK/RID/运行时版本确定。

正式 JSON Schema 验证需要 Python 与 `scripts/requirements-validation.txt`；运行 `scripts/check-contracts.py snapshot FILE...` 或对应 manifest/diff 类型。Core 还检查唯一 ID、图关系、范围、覆盖率等语义；JSON Schema 通过不能替代这些检查。原有库的 CMake/CTest 保持独立执行。
