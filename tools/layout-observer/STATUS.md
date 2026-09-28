# 实现与验证记录

2026-09-28，开发分支 `codex/cross-language-layout`。原始签名库与 admission 未修改；新实现位于本目录。D0 设计审查之后，D1–D5 已形成可运行后端与共同协议，支持范围按下表的实际执行环境确定。

| 阶段 | 当前实现与验证 |
| --- | --- |
| D1 值类型闭环 | Linux x64 P2996/C# Debug/Release 四单元实际执行；6 个共享案例跨语言与跨配置比较；schema/Core/CLI 与错误路径验证 |
| D2 对象与报告 | Windows x64 CoreCLR 10.0.12 冻结快照，13 个根实例；工厂、私有继承、装箱、引用、数组/字符串、压缩 GC 后重定位；7 类故障路径；离线 HTML 结构/转义/几何检查 |
| D3 MSVC | MSVC 19.44.35229，DIA 14.44.35215.0；x64/x86 各 Debug/Release 的实际 PE/PDB；GUID/age 校验、私有成员/继承/位域和独立 oracle |
| D4 AOT/架构 | .NET SDK 10.0.401、runtime 10.0.12；Windows x64/x86 的 CoreCLR 与 NativeAOT 均真实发布执行；AOT 0 警告；含引用字段类型跨架构确有差异 |
| D5 封送/复杂类型 | 明确 runtime-marshalling profile、原生 C++20 接收端、真实 P/Invoke 双向正确/错误哨兵检查；union/位域/空子对象/opaque/非平凡类型保留覆盖边界 |

P2996 首个工具链为 Bloomberg Clang 21.0.0git、commit `9ffb96e3ce362289008e14ad2a79a249f58aa90a`、libc++ 210000。本机 WSL 的 glibc 旧于该工具链，验证使用隔离的匹配运行时与 loader/rpath，未替换系统库。普通 Clang 不具备所需反射 API 时配置会失败。

ClrMD 固定为 4.1.745802，Roslyn generator package 为 4.14.0；NuGet transitive dependencies 有 lock 文件。Linux 原库回归使用 CMake 3.31.10，避免旧 CMake 3.22.1 无法识别 CXX26；4/4 既有测试通过，包含 admission 负向编译检查。

审查修复包含：递归原点/视图/profile 闸门、数组子布局遗漏、枚举/数组内联循环、not-applicable 覆盖率绕过、未知值假相等、隐藏区域遗漏、空校准证据、范围并集、主进程退出后管道无界等待、数组未分类区域误绘 padding。对应回归位于 CoreChecks/ReportChecks/ProcessChecks；真实 CLI 错误路径位于 `scripts/check-integration.py`。

最终核心检查为 54 项，报告检查 19 项；进程检查在 Windows/Linux 各 4 项，CLI 集成 16 项，均通过。Linux 四单元通过统一调度器重新构建、采集和比较，四份 comparison 各 6 个案例全部 same/complete。外部程序集的私有 readonly 类型注册也在 JIT/NativeAOT 实际执行，使用自己的编译身份，未混入内置案例。Windows/Linux 的真实 P/Invoke 三组 profile 均通过尺寸、偏移和正反哨兵检查。

未计入通过的范围：

- ARM64 没有实际 runner；请求该必需单元会失败，不把交叉编译视为已执行。
- CoreCLR 对象适配器仅声明已校准的 CoreCLR 10；其他 runtime/任意业务进程/自定义实例工厂需补适配和证据。
- MSVC/DIA 未提供的类型 alignment、无法证明的虚基类/隐藏区域、重叠子对象独占范围仍然 unknown 或 unsupported。
- 自定义封送、源生成 marshaller、NativeAOT 的封送表示不套用 runtime-marshalling profile。
- HTML 已通过离线结构、编码、未知区域与几何测试；当前浏览器工具的访问限制使目视验收未执行，不能宣称浏览器视觉验收通过。

测试产物、SDK、编译器、DAC/PDB、dump 和个人运行日志留在 ignored build/bin/artifacts 目录，不提交为跨机器基准。基准需要用户显式选择目标环境、scope/policy 和输入快照。
