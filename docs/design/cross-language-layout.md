# 跨语言、跨构建布局观测器

日期：2026-09-28。状态：设计审查后已实现独立工具和 D1–D5 后端闭环；具体已执行矩阵与保留边界见[实现记录](../../tools/layout-observer/STATUS.md)，使用入口见[Layout Observer](../../tools/layout-observer/README.md)。本文保留设计决策，实际机器契约以[协议 0.1](../../tools/layout-observer/contracts/PROTOCOL.md)为准。

开发分支：`codex/cross-language-layout`。代码审查基线：`76304981d38be218b0514507907d756c12dfd39b`。本工具属于 TypeLayout，独立于任何教程。审查发现及处置见[审查记录](cross-language-layout-review.md)。

## 1. 目标与交付边界

输入类型对应关系、构建配置以及必要的实例工厂，采集 C++ / C# 在确定环境中的类型表示，输出可复查的快照、逐字段差异、字节布局图。支持同类型跨构建、跨语言对应类型、指定封送规则下的布局比较。

首要用途为观测和诊断；CI 使用相同事实与比较规则。报告回答“哪些事实已知、哪里不同、依据是什么”，不把布局一致解释为调用 ABI、序列化、对象生命周期或字节复制安全的证明。现有 TypeLayout admission 继续承担它原有的职责。

完整目标包括 C++ 反射采集、MSVC 实际产物、CoreCLR 值与对象、NativeAOT 静态探针、封送表示和离线报告。采用分阶段验证；未通过闸门的能力明确显示未支持，不用较小的首阶段代替完整目标。

本方案不分析优化后局部变量的寄存器分配，不实现堆保留大小分析，不自动把任意 C++ 指针解释为所有权。首版对象视图展示浅层布局和引用槽；引用目标的递归对象图是后续独立能力。

## 2. 与现有库的关系

保留 `get_layout_signature<T>()`、`SigExporter`、`.sig.hpp`、`is_byte_copy_safe_v<T>` 及现有 CI 的语义。新工具不能通过删除 `SigExporter::add` 的门禁来支持普通对象，也不能把 `add_relocatable` 当作绕过入口。

新增独立的 observation 描述器和 JSON exporter，复用可验证的反射枚举能力。完整描述来自直接遍历类型、成员和基类，而非解析旧签名重建树：旧签名有意丢弃名称、展平嵌套/基类，并折叠某些字节数组；其标量对齐常量、空类型占位规则也不适合作为通用观测证据。

旧签名可附带为 `legacySignature`，仅用于追踪原有结果。导入旧签名时标为有损，不凭空恢复名字、层级、实际空子对象占位或隐藏字段。仅存在 `vptr` 标志也不能推出指针数量和偏移。

新 exporter 从实际 `sizeof(T)`、`alignof(T)` 和具体编译器反射取得事实，分别记录独立类型与子对象上下文。发现旧库表示边界时另立修复议题；本设计阶段不顺带修改旧算法或既有 golden signatures。

## 3. 架构与代码边界

```mermaid
flowchart LR
    M[类型映射与构建配置] --> R[构建与采集调度]
    R --> N[C++ collector]
    R --> C[C# collector]
    N --> S[带证据的 JSON 快照]
    C --> S
    S --> V[验证与规范化]
    V --> D[按比较规则逐项 diff]
    D --> O[CLI / JSON / 离线 HTML]
```

建议组件位置如下，均为后续实现规划，并非本次已添加的模块：

| 位置 | 职责 |
| --- | --- |
| `tools/layout-observer/contracts/` | snapshot、manifest、comparison 的版本化 JSON Schema 和契约 fixtures |
| `tools/layout-observer/native/` | 独立 CMake 项目、C++ observation 描述与导出、后端适配 |
| `tools/layout-observer/dotnet/` | C# Core、CLI、静态探针生成器、CoreCLR / DIA 适配项目 |
| `tools/layout-observer/report/` | 自包含 HTML 模板及资源，不依赖在线服务 |
| `tools/layout-observer/cases/` | 对应类型、实例工厂、构建配置和预期比较结果 |

Core 只处理 JSON 和比较，不引用 ClrMD、DIA、Roslyn。CLI 通过显式后端加载它们；纯 diff/report 不需要 C++ 编译器或目标进程。初始控制端采用 .NET 10 与 `System.Text.Json`，原生 collector 是独立可执行程序。

原有 header-only 用户无需安装 .NET。新工具单独 configure/build；若以后增加根 CMake 入口，选项默认关闭。CI 新增独立 workflow，不把诊断依赖加入原有库的默认构建。

## 4. 采集后端与验证闸门

### 4.1 C++ 反射后端

- 首个可执行闭环使用仓库已经配置的 Linux x64 P2996 工具链及同目标的 CoreCLR；不是假定普通 Clang/MSVC 已支持反射。
- 固定实际编译器 commit/版本、标准库、容器 digest、target triple 和编译参数；现有 `latest` 镜像名字只能用作发现入口，不能作为可复现身份。
- observation 描述器保存成员名称、声明顺序、类型、基类和实际位偏移。普通非平凡类型可被观测；是否满足 byte-copy admission 是独立属性。
- C++ 引用成员的存储宽度不能用 `sizeof(T&)` 测量：该表达式给出被引用类型的大小。引用槽宽度必须来自已验证的后端/ABI 依据，否则 unknown；不直接沿用旧签名的指针宽度假设。
- 反射无法给出的虚表指针等隐藏区域标为未知。虚继承在当前库中会触发编译期拒绝；必须在实例化描述器前分流，输出稳定的 unsupported 原因，避免一个类型让整批采集失败。
- union / 位域 / 空基类 / `[[no_unique_address]]` 按能力报告。不能把 `sizeof(Base)` 直接当作基类在宿主中独占的范围；不能把任何空成员统一当作零字节。
- opaque 类型保留边界及声明来源；相同 tag/size/align 只说明已知外形一致，不能证明内部字段或表示一致。默认要求完整字段表示的 policy 对 opaque 内部返回 incomplete；显式外形 scope 的结果必须注明范围。
- 能力探针失败与普通构建失败分开：前者形成明确的能力记录，后者保留日志并失败；不静默切换编译器或省略请求类型。

### 4.2 C# 静态探针

针对注册的闭合类型生成代码，首先支持可访问实例字段的受控类型。`Unsafe.SizeOf<T>()` 记录托管值表示；字段偏移通过同一个实例内部的 managed byref 计算；数组步长通过同一个数组内相邻元素的 byref 计算。

不得先转换成裸地址，再跨 GC 或异步点相减。不得跨两个对象推导字段偏移。不得用字段值的字节搜索定位字段；padding 的实际字节内容不稳定，也不应读入签名。

静态生成器不能天然访问第三方私有字段：受控 partial 类型可显式集成生成代码，其他情况需注册可访问的适配器或使用 CoreCLR 诊断后端。不可静默遗漏私有/继承字段后宣称完整。属性是方法；若比较自动属性，明确绑定实际 backing field，不执行 getter。静态字段不计入实例布局。

`Unsafe.SizeOf<Class>()` 只表示引用槽宽度。C# 没有可通用代替 `alignof` 的这个 API；观察到的某一地址对齐、包装结构字段偏移、`Pack` 参数都不能自动当作类型的真实 alignment。没有可靠依据时保存 unknown。

`ref classVariable` 指向引用槽，不能作对象本体原点；class 静态探针只能提供字段间的相对偏移，若使用则须标为 `anchor-field` 原点，不能补猜绝对偏移。`Class[]` 的元素步长是引用槽步长，不是 class 对象大小。

类型实例、嵌入值、数组元素、装箱数据分别记录 context。探针可使原本被优化掉的值实体化，结果只描述该观测上下文，不声称重现未插桩程序的局部存储策略。

### 4.3 CoreCLR 对象后端

受控子进程调用用户注册的实例工厂，使用有稳定逻辑 ID 的强根登记待观测实例。到达采集屏障后，外部采集器获取一致性快照/转储，再由固定版本的 ClrMD 与匹配 runtime/DAC 读取字段和对象信息。

不通过任意反射构造业务对象，也不默认跳过构造函数创建不合法实例。工厂出错、快照不一致、DAC 不匹配或类型未加载，都返回诊断。采集超时后释放本工具拥有的子进程；不向用户的普通应用注入代码。

通过带 ID 的 rooted wrapper 在快照内部重新定位实例，不把 producer 先前输出的地址交给稍后 attach 使用。普通强根只保活，不保证地址固定；不假定任意含引用类型可以 pin。禁止读取未冻结且正在变化的 live heap。

字段偏移使用 ClrMD 对应 API 的明确口径，不将其 `Offset` 原样解释为 CLR 对象引用起点偏移。对象引用起点、实例数据起点、分配区域起点之间的转换只能由绑定运行时的适配器提供。对象头包含状态的值不进入类型签名。

`ClrObject.Size` 的报告值记录为 `runtimeReportedObjectBytes`，保留 API 和版本证据；完成该 runtime 的校准后，才能标注它是否包括对象头、最小对象大小及尾部对齐。不得通过一次 `GC.GetAllocatedBytesForCurrentThread` 差值推导对象大小，也不把 shallow size 叫作独占/保留堆大小。

### 4.4 MSVC、NativeAOT 与封送后端

| 后端 | 实现约束 | 加入支持矩阵的闸门 |
| --- | --- | --- |
| MSVC / DIA | 分析实际匹配的 PE/PDB，校验产物与符号身份；保留编译单元和宏上下文，不用 Clang 重编译结果冒充 MSVC | Debug、带符号 Release 的字段/位域/继承布局与独立 C++ 探针交叉核对；缺失信息可准确识别 |
| NativeAOT | 复用可静态编译的值类型探针，显式保留注册类型和实例；不依赖 Reflection.Emit 或默认复用 CoreCLR/DAC | 真正 publish 后在目标架构运行，验证裁剪后字段覆盖及尺寸；普通对象头/大小另行验证，未支持时标明 |
| 内置封送器 | 初期只支持明确声明的固定大小 formatted 类型，用 Marshal API 取得对应非托管表示 | 与实际 P/Invoke fixture 的字段值传递交叉验证；注明这只验证该 fixture 与封送路径 |
| 自定义/源生成封送器 | 表示可能依赖实例和用户代码，必须使用专门适配器 | 缺适配器即 unsupported，不用 Marshal 默认结果代替 |

MSVC 的现有示例签名是模拟数据，不能作为此后端已支持的证据。DIA 不能提供的 alignment 仍为 unknown。DWARF 作为后续同接口扩展，不与首个闭环同时铺开。

## 5. 数据契约

### 5.1 原始事实与比较映射分离

snapshot 不绑定某一组跨语言业务映射。它记录产物中的 `typeId`、`memberId`；manifest 中的 `caseId`、逻辑字段 ID 将两个快照的具体字段对应起来。字段映射在比较阶段加入，不能改变原始采集事实。重复 ID、歧义名称、重复映射或未提供所需跨语言对应关系属于配置错误。

regression 中，枚举完整且基线存在、新产物不存在的字段为 `removed → different`，新增同理；不能先作为“映射失败”终止。枚举不完整时缺失只能记为待确认，影响 coverage。生成器先枚举实际存在的字段，不因基线字段已删除就生成引用不存在成员的代码。用户显式声明重命名对应关系，否则不自动把删除/新增合并为改名。

同一个类型在不同 context 可有多个 observation；包含泛型实参、声明程序集/模块、源类型身份。跨构建不假定 metadata token 或编译器修饰名稳定。展示名可用于候选提示，不能自动证明语义相同。

### 5.2 Snapshot 结构

```text
Snapshot
  schemaVersion / snapshotId
  producer: id, version, capabilityProfile
  build: buildId, runId, sourceRevision, sourceDirty, sourceDigest, requestedProfile
         artifactDigest, compiler/runtime identity, target ABI/architecture
         pointerBits, bitsPerByte, endian, flags/macros, dependency identities
  typeDescriptors[]
    id, kind: scalar | enum | record | union | array | reference | opaque
    representation: widthBits, signedness, encoding, floatingFormat
    referenceKind, targetTypeRef, elementTypeRef, fixedCount, enumUnderlyingTypeRef
  observations[]
    id, typeId, displayName, genericArguments
    view: native | managed | marshaled
    context: kind, hostObservationId, hostMemberId, elementIndex
    instanceShape: length, dimensions
    origin: kind, extentStartBit, conversionEvidence
    metrics: valueSizeBytes, standaloneSizeBytes, referenceSlotBytes
             arrayStrideBytes, runtimeReportedObjectBytes, alignmentBytes
    members[]: id, declarationOrder, role, typeRef, offsetBits
               bitWidth, declaredTypeSizeBits, occupiedRanges, overlapGroup, childObservationId
    runtimeRegions[]: role, ranges, evidence
    coverage: fieldEnumeration, extent, occupiedRanges, hiddenRegions
    limitations[]
```

各 metric/range 采用统一的 Fact 包装：`state = known | unknown | not-applicable`，known 必须有 value 与 evidence；unknown 必须有 reason；not-applicable 必须能从 context 解释。evidence 含 `kind = compiler | runtime | derived | fixture`、采集方法及其版本、依赖事实 ID。语言保证另存为规则说明，不拿来冒充此次测量。

所有位偏移相对于明确的 origin，存有符号整数；对象头可位于负偏移。首批目标限制 `bitsPerByte = 8`，不支持的模型拒绝进入相应比较规则。位域还需记录后端位编号约定及规范化依据；不能仅用 endian 猜测位域编号。

`typeId/typeRef` 指向快照内 `typeDescriptors`，递归类型用 ID 引用而不是无限展开。representation 的属性使用 Fact，保留实际宽度、符号、表示/编码依据；referenceKind 区分 native-pointer、native-reference、GC-reference、function-pointer 和 member-pointer。引用槽自身的 descriptor 与 `targetTypeRef` 分开；数组保留元素类型与定长，enum 保留底层类型。位域有效 `bitWidth` 与声明存储类型大小分开；不能仅凭 `int`、`bool`、`char` 等语言名称归一化。缺失 policy 要求的类型表示会降低 coverage，manifest 不能把未知表示强制映射成相同。

context.kind 取 `complete-value | embedded-value | array-element | heap-object | boxed-value`。内嵌成员/基类通过 `childObservationId` 连接实际子对象观测，子观测的 hostObservationId/hostMemberId 回指所属位置；数组元素另有 elementIndex。内联关系必须无环且双向一致，类型引用则允许递归。相同 typeId 在不同宿主的 observation 不合并，也不用 standalone 观测替换 embedded 观测。运行时数组/字符串通过 instanceShape 保存长度/维度；不把实例长度写成类型固定长度。

大小口径不可混用：独立类型大小不等于子对象独占范围；值大小、引用大小、数组步长、运行时对象大小分别存在。数组/字符串总大小是实例事实，要记录长度；不能提升为该类型的固定 size。引用目标由 reference descriptor 的 `targetTypeRef` 表示，不作为内联 childObservation，也不递归并入拥有者 extent。

### 5.3 区域与 padding

保留成员树，并另外提供 range 投影。union、Explicit、基类尾部复用等允许范围重叠；`occupiedRanges` 可以未知，不简单使用 `offset + sizeof(fieldType)` 推导全部子对象占位。

只有 extent 和相关占用范围均完整时，才计算 padding：对已知占用范围取并集，再求补集。嵌套层级各自计算，汇总时不重复计算父子区域；union 不把各成员大小相加。存在未知隐藏区域时，空白显示“未分类区域”，不涂成 padding。不能通过原始内存字节相同与否比较 padding。

以下仅为未来 schema 的设计 fixture，不是本轮采集输出：

```json
{
  "id": "fixture:managed-sample",
  "view": "managed",
  "context": { "kind": "complete-value" },
  "origin": { "kind": "value-start", "extentStartBit": 0 },
  "metrics": {
    "valueSizeBytes": {
      "state": "known", "value": 12,
      "evidence": { "kind": "fixture", "method": "pack4-scalar-case-v1" }
    },
    "alignmentBytes": { "state": "unknown", "reason": "collector-does-not-report-type-alignment" }
  },
  "coverage": { "fieldEnumeration": "complete", "hiddenRegions": "not-applicable" }
}
```

这只是 observation 摘录，D1 须形成完整 schema、完整正例和非法样例；不能拿摘录通过 JSON 语法检查来代替协议验证。

## 6. 比较规则、结果和 CI

先校验 schema 和事实，再匹配类型/字段，选择 scope 与 policy，最后逐项比较。scope 明确比较字段数据、完整值，还是运行时对象；不能默默剔除对象头以获得相同结果。

| 模式 | 比较规则 |
| --- | --- |
| regression | 同一映射类型跨构建，按选定 scope 检查增删字段、尺寸、偏移、表示、重叠和相关运行时区域 |
| representation | 显式映射的 C++ native 与 C# managed；允许比较原点已证明可归一化的值表示，指出引用/运行时差异 |
| marshaled-layout | C++ native 与特定 marshalling profile；固定大小布局约定检查，不授予一般 ABI 兼容结论 |

首个默认 policy 为 `value-fields-v1`：要求完整字段枚举、选定值 extent、映射字段的偏移/表示/重叠关系。数组 scope 额外要求 stride。alignment 独立展示，不在此 policy 的完整性要求内；更严格的 `value-alignment-v1` 将其列为必需。输出必须写“在此 scope/policy 下相同”，不能把有限 policy 的相同扩展到所有布局属性。

每个结果包含 `verdict` 和独立的 `coverage`。若比较前提不成立，给 `not-comparable`；否则任一已知必需属性不同即为 `different`，即使还有未知属性；没有已知差异但必需信息缺失为 `incomplete`；只有必需信息完整且一致才为 `same`。已知差异不能被 incomplete 吞掉。

字段槽宽度相同但一个为原生指针、另一个为 GC reference，物理范围可以相同，representation 必须报告不同。整数符号、浮点表示、字符/布尔编码、字节序不能仅由字段宽度推导。只有几何范围的投影可提供附加的 shape 对照。

退出码规划：0 = 所有请求在指定 policy 下 same；1 = 有 different 且无更高优先级问题；2 = 有 incomplete / not-comparable / 未支持的必需能力；3 = 配置、协议、构建或采集错误。批量任务使用最高优先级退出码，保留每项已知差异；expected-difference fixtures 由测试驱动按预期验证。`capture` 成功只表示采集成功，不能代替 `compare` 的判定。

CI baseline 必须显式指定，不能把本次结果自动更新为基准。必需矩阵单元未执行、产物缺失、出现重复 observation 都不算通过。

## 7. 可复现性与调度

manifest 初期只使用 JSON，避免同时维护 YAML 和 JSON 两套契约。声明 profile、参数数组、工作目录、目标执行器、超时、产物、类型映射和必需能力。命令以 executable + argument array 执行，不拼接 shell 字符串；manifest 是用户主动执行的项目配置。

每个 profile 使用独立目录。采集器报告实际 compiler/runtime、进程架构和加载的模块；与请求配置不符时失败。C++ 捕获相关编译单元配置，C# 保留程序集身份和实际 runtime 身份。重编译专用探针时须记录其与目标项目的配置关系，不能无条件称为原业务产物的布局。

每次采集使用唯一 `buildId/runId` 及独立产物路径，不沿用旧 exporter 的 arch/os/compiler 文件名作为唯一键。同平台 Debug/Release、不同编译器版本、重复执行都不能覆盖已有快照；布局内容身份独立于 runId，支持跨运行稳定比较。

源码版本、dirty 内容摘要、产物摘要、依赖 lock、容器 digest、collector 版本组成环境身份。快照允许携带可诊断的构建元数据，但不收集全部环境变量、凭据或任意业务内存；共享报告使用相对路径。地址、时间戳、临时路径、对象头状态不进入布局内容指纹。

初期可直接做结构化比较，不急于增加多个 hash。后续指纹用于缓存和分组：geometry、representation、environment 分开，编码带 schema/policy/coverage 版本。未知值不能生成可用于“完整一致”判定的指纹；hash 命中仍需结构化复核。

跨架构构建与运行分开记录；没有可执行目标的机器只可输出编译器能证明的事实。不得把未运行的 managed collector 标为成功。Debug/Release 无差异是有效结果；多个变量同时改变时只报告关联，不能自动认定优化等级是原因。

## 8. 报告

交付 JSON 原始快照、JSON diff、简洁终端摘要和自包含 HTML。HTML 离线打开，支持两个构建并排、嵌套展开、重叠层切换、字段筛选、证据和未知项查看；所有类型名、路径和诊断文字按不可信文本转义，不加载远程脚本。

总大小、字段数据、运行时区域、padding、未分类区域使用不同表示。引用槽链接到类型说明；不默认递归跟踪实例。报告并列展示 requested 与 actual 构建环境、scope/policy、verdict/coverage，避免一张绿色图掩盖未测属性。

## 9. 实施顺序与阶段验收

下列为分阶段交付及验收要求。D0 完成后按此顺序实施；当前实际验证范围、未执行平台和限制见[实现记录](../../tools/layout-observer/STATUS.md)，不能将一个平台的通过扩展到所有环境。

| 阶段 | 交付 | 必须通过的验收 |
| --- | --- | --- |
| D0 方案审查 | 本文、审查记录、能力边界和风险闸门 | 对照代码确认限制；文档链接、JSON 片段与 diff 检查通过；提交开发分支 |
| D1 值类型闭环 | schema、validator、native observation exporter、C# 静态探针、manifest、CLI diff | 同 Linux x64 的 native/managed Debug+Release 四个单元实际采集；Pack/字段重排/嵌套/数组结果正确；未知 alignment 在严格 policy 下不能得到 same |
| D2 CoreCLR 与报告 | 实例工厂、稳定快照、ClrMD、离线 HTML | 空 class、继承、含引用 struct、装箱、byte[1] 和不同长度 string/数组分别校准原点与大小；强制压缩 GC 后仍按逻辑 ID 正确定位；缺 DAC、工厂失败与私有字段遗漏可诊断；图中未知区域不冒充 padding |
| D3 MSVC 实际构建 | PE/PDB/DIA collector 与 Windows 同目标比较 | 符号身份匹配、Debug/Release 实测、字段偏移及已知尺寸与原生独立探针一致；模拟签名不能充作 evidence |
| D4 AOT 与更多架构 | NativeAOT 值探针，ARM64 / x86 按后端支持逐项加入 | publish 后目标执行；实际架构校验；裁剪能力核对；跨架构标量对齐使用实际值；无 runner 的必需单元使检查不通过 |
| D5 封送与复杂表示 | 指定封送 profile、union/位域/空子对象等后端补全 | bool/char 等表示差异被识别；P/Invoke fixture 交叉核对；未知虚布局/自定义封送器有明确诊断；所有既有能力回归通过 |

D1 先利用已有反射工具链建立事实链；这不意味着将 MSVC、class 或 NativeAOT 从目标删除。阶段受阻应提交具体证据和未满足闸门，不用旧签名模拟输出或降低完整性标准通过。

测试必须有独立依据：标准布局 C++ 使用 `sizeof/alignof/offsetof`；C# blittable fixture 的 managed 探针与指定封送视图交叉核对；CoreCLR 与固定 runtime 的 SOS/诊断输出校准。不能只让 exporter 和 diff 共用同一错误模型互相证明。

契约测试至少覆盖：两侧都未知；已知差异并存未知；不同原点；重复/缺失字段；同几何不同引用种类；重叠不重复计数；嵌套与平铺可区分；无差异；旧签名有损导入；产物/符号不匹配；unsupported 类型不拖垮其余采集；旧签名与 admission 的现有回归不变。

另验证同一类型在不同宿主中的子 observation 不混淆；映射歧义为错误、完整枚举下字段增删为差异、不完整枚举下缺失为待确认；同宽度但有无符号/字符编码/引用种类不同能被识别，未知表示不能由名称或 manifest 强制判等。

同平台多构建产物防覆盖、opaque 与正常类型混合采集、非平凡普通对象可观测但不放宽旧门禁、报告文本转义都进入验收。NativeAOT 的 private/泛型已知类型实际发布验证；裁剪/AOT 警告必须为零或逐条解释其对覆盖率的影响。

## 10. 需用原型关闭的风险

| 风险 | 关闭方法 | 未关闭时的输出 |
| --- | --- | --- |
| P2996 版本间能力不同 | 最小成员/基类/位域能力程序，固定通过的工具链身份 | 后端能力不足或明确 unsupported |
| 子对象占用与隐藏区域不完整 | 各后端独立校准，保留 standalone/embedded 区别 | occupiedRanges unknown，禁止推断完整 padding |
| 私有字段与 AOT 裁剪 | 明确类型注册/访问策略，发布后执行，校验字段枚举完整性 | 缺字段/能力不足，不返回完整一致 |
| ClrMD 对象大小与原点口径 | 固定 runtime/DAC，空对象/装箱/数组/继承 fixtures | 保留 runtimeReported 值与未分类区域 |
| DIA 缺失布局属性 | 实际 PDB 加原生探针对照，记录符号身份 | 缺失属性 unknown 或按 policy 判 incomplete |
| 自定义封送表示依赖实例 | 封送器专用适配与实例契约 | 不支持该 profile，不套用默认 Marshal 布局 |

## 11. 依据

以下源码在上述基线核对，外部文档核对日期为 2026-09-28。实现时固定依赖版本并复核 API；这些链接不等于后端已验证。

- [旧签名入口](../../include/boost/typelayout/signature.hpp)、[展平与空类型处理](../../include/boost/typelayout/detail/signature_impl.hpp)、[基础类型映射](../../include/boost/typelayout/detail/type_map.hpp)、[签名回归](../../test/test_core.cpp)。
- [导出门禁](../../include/boost/typelayout/tools/sig_export.hpp)、[现有反射工具链流水线](../../.github/workflows/compat-pipeline.yml)、[MSVC 模拟签名](../../example/sigs/x86_64_windows_msvc.sig.hpp)。
- [Unsafe.SizeOf](https://learn.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.unsafe.sizeof?view=net-10.0)、[Unsafe.ByteOffset](https://learn.microsoft.com/en-us/dotnet/api/system.runtime.compilerservices.unsafe.byteoffset?view=net-10.0)、[StructLayout](https://learn.microsoft.com/en-us/dotnet/api/system.runtime.interopservices.structlayoutattribute?view=net-10.0)、[Marshal.OffsetOf](https://learn.microsoft.com/en-us/dotnet/api/system.runtime.interopservices.marshal.offsetof?view=net-10.0)。
- [ClrMD](https://github.com/microsoft/clrmd)、[ClrInstanceField 的地址口径](https://github.com/microsoft/clrmd/blob/main/src/Microsoft.Diagnostics.Runtime/ClrInstanceField.cs)、[ClrObject.Size](https://github.com/microsoft/clrmd/blob/main/src/Microsoft.Diagnostics.Runtime/ClrObject.cs)。
- [ClrMD heap 大小与对齐实现](https://github.com/microsoft/clrmd/blob/main/src/Microsoft.Diagnostics.Runtime/ClrHeap.cs)、[ClrMD 快照与 DAC 前提](https://github.com/microsoft/clrmd/blob/main/doc/FAQ.md)。
- [NativeAOT 限制](https://learn.microsoft.com/en-us/dotnet/core/deploying/native-aot/)、[DIA 字段偏移](https://learn.microsoft.com/en-us/visualstudio/debugger/debug-interface-access/idiasymbol-get-offset?view=visualstudio)。
