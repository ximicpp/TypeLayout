# 跨语言布局方案审查记录

日期：2026-09-28。审查对象：对话中的初步方案，以及 TypeLayout 基线 `76304981d38be218b0514507907d756c12dfd39b`。

当前交付是[优化后的设计](cross-language-layout.md)，并非功能实现。下文“已修订”表示设计消除了对应假设；后端能否正确采集仍须通过设计中的阶段验收。

## 结论

方案可推进，但不能直接给现有 `.sig.hpp` 增加一个 C# producer 就获得完整工具。核心应为独立 observation 协议和可声明能力的采集器，保留现有兼容性库的契约。首个闭环用仓库已有的 Linux x64 反射工具链；实际 MSVC、CoreCLR 对象、NativeAOT 和封送均保留为明确开发阶段。

审查分别覆盖现有 C++ 实现、C# 运行时可行性、数据与比较契约。初步方案中影响正确性的修订如下。

## 发现与处置

| ID / 优先级 | 初步方案的问题与证据 | 修订决策 | 验证阶段 |
| --- | --- | --- | --- |
| R01 / P1 | 旧签名不能恢复名称、嵌套和基类树。`signature.hpp` 明确 flattened/no names；`test_core.cpp` 明确断言 Nested=Flat、Derived=Flat | 独立反射遍历生成 observation；旧签名仅附带或有损导入，不作为完整模型来源 | D1 |
| R02 / P1 | `SigExporter::add` 要求 trivially copyable；`add_relocatable` 仍有 byte-copy gate，不能直接覆盖普通对象 | 新增观测 exporter，保留旧 gate；可观测性、布局一致、可按字节搬运分别处理 | D1 |
| R03 / P1 | `detail/type_map.hpp` 固定宽度标量的 align 常量不能代表所有 ABI；`signature_impl.hpp` 空成员展平也不能证明宿主中的实际占位 | 新描述器读实际 sizeof/alignof；standalone 与 embedded 分开；占用未知时不填假值，不借此次设计改旧引擎 | D1、D4、D5 |
| R04 / P1 | `example/sigs/x86_64_windows_msvc.sig.hpp` 明确是 simulated sample；配置 `CMAKE_CXX_STANDARD 26` 也不证明任意编译器支持反射 | 首先锁定真实通过能力探针的反射工具链；MSVC 必须实际采集匹配 PE/PDB，模拟数据仅作 fixture | D1、D3 |
| R05 / P1 | `vptr` 标记不包含位置/数量，虚继承会 static_assert；把字段间空白全称 padding 会隐藏运行时区域 | 预检/分流 unsupported 类型；保留 hiddenRegions/occupiedRanges coverage，未知空白不分类为 padding | D1、D5 |
| R06 / P1 | Unsafe.SizeOf、Marshal.SizeOf/OffsetOf、class 对象大小不是同一个口径；非 blittable Sequential 不能代替托管实测 | 三种 view 独立；referenceSlot/value/stride/runtimeReportedObject 分开；只按指定封送 profile 检查 | D1、D2、D5 |
| R07 / P1 | 泛称“静态生成探针”没有解决私有字段、继承、GC 地址移动与 AOT 裁剪 | 定义类型注册与访问策略；同实例 managed byref；实际发布执行；遗漏必需字段必须影响 coverage | D1、D2、D4 |
| R08 / P1 | “对象起点”和“对象大小”缺少后端口径；首次字段归零或用分配差值估算会误判 | 明确 origin、版本化转换证据和 runtimeReported 大小；受控实例工厂+一致性快照，固定 runtime/DAC 校准 | D2 |
| R09 / P1 | 单一 same/different/unknown 状态会让未知属性掩盖已知差异，也会使 C# 未知 alignment 导致所有比较无用 | verdict 与 coverage 独立；定义 scope/policy 必需属性；已知不同仍报告 different，严格 alignment policy 保留 incomplete | D1 |
| R10 / P1 | 把字段宽度、sizeof 和完整区域简单相加，会错误处理 union、重叠字段、空基类和尾部复用 | 保留层级/ranges，声明类型大小与实际占用分开；完整区域才计算并集补集，禁止父子重复计数 | D1、D5 |
| R11 / P2 | 把 caseId 存进原始采集结果，会让一次快照绑定某个跨语言匹配；hash 含构建标签又会制造虚假差异 | manifest 负责业务映射；事实 ID 保留来源；先逐项比较，后加可分离的 geometry/representation/environment 指纹 | D1 |
| R12 / P2 | 初期同时实现所有后端、构建调度和完整对象图，难以验证每一层；依赖可能侵入 header-only 用户 | 明确 D1–D5 闸门，先完成可验证闭环；独立 CMake/.NET 项目；离线报告先展示浅层布局 | 全阶段 |
| R13 / P1 | 仅存 Debug/Release、容器 latest 或源码 SHA，不足以复现真实布局；探针重编译也可能与业务产物配置不一致 | 保存 requested/actual、dirty/source/artifact digest、编译单元/程序集配置、依赖和工具链身份；缺 runner 不算运行成功 | D1–D4 |
| R14 / P1 | 默认 Marshal 结果不能代表源生成/自定义封送，也不能证明函数调用 ABI | 固定大小内置封送先行；自定义 profile 需专用适配；实际 P/Invoke fixture 只支持限定结论 | D5 |
| R15 / P1 | 第二轮发现 typeRef 只有身份而缺少实际叶类型表示，无法实现符号/编码/指针种类比较 | 增加 typeDescriptors，槽类型与 targetTypeRef 分开，scalar/enum/array 属性含 evidence 和 unknown；opaque 内部缺失不判完整一致 | D1 |
| R16 / P1 | 第二轮发现 context 只有枚举，无法把嵌入观测关联到实际宿主和成员 | context 增加 hostObservationId/hostMemberId/elementIndex，成员连 childObservationId；实例长度独立；禁止用 standalone 替代 embedded | D1、D2 |
| R17 / P1 | 第二轮发现“缺少映射字段报错”会掩盖 regression 中真实字段删除 | 映射歧义为配置错误，枚举完整时新增/删除为 different，不完整时缺失待确认；生成器按实际字段生成 | D1 |

## 开发前必须保持的决策

- 不改变旧签名与 admission 的既有语义；新功能单独构建。
- unknown、not-applicable、unsupported、采集失败有不同含义；不转换成零或成功。
- scope/policy、origin、evidence 和 coverage 是协议核心，不是报告层补充文字。
- 同名不自动映射，布局相同不自动授予互传、序列化或复制权限。
- D1 的有限闭环只验证架构，不宣告完整方案完成；后续闸门失败需诊断，不用模拟输出替代。

## 本轮验证与后续状态

本轮只添加设计和审查文档；没有修改库源码、构建配置、测试或签名样例，也没有运行并宣称任何新 collector 可用。第二轮按原生后端、托管后端、比较协议三个方向交叉审查，新增 R15–R17 已纳入设计；C# 后端复核未发现新的设计阻塞。文档验证检查本地 Markdown 链接、JSON 摘录语法、代码围栏及 `git diff --check`。本轮不以文档修改触发大型编译工具链安装。

D0 的交付是可供实现的决策和验收条件。D1–D5 均未开始；风险清单仍是必须用原型和独立依据关闭的事项。

## 证据导航

- [签名入口](../../include/boost/typelayout/signature.hpp)、[签名布局实现](../../include/boost/typelayout/detail/signature_impl.hpp)、[标量映射](../../include/boost/typelayout/detail/type_map.hpp)。
- [导出器](../../include/boost/typelayout/tools/sig_export.hpp)、[反射辅助代码](../../include/boost/typelayout/detail/reflect.hpp)、[现有断言](../../test/test_core.cpp)。
- [流水线](../../.github/workflows/compat-pipeline.yml)、[模拟 MSVC 签名](../../example/sigs/x86_64_windows_msvc.sig.hpp)。
- C#、ClrMD、DIA、NativeAOT 的官方资料及核对日期集中列于[设计依据](cross-language-layout.md#11-依据)。
