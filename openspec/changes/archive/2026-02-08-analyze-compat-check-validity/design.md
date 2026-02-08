## Context

Boost.TypeLayout 的跨平台兼容检查（Cross-Platform Compatibility Check）基于以下核心假设：

> **V1**: `layout_sig(T) == layout_sig(U) ⟹ memcmp-compatible(T, U)`

即：如果两个平台上同一类型的 Layout 签名完全相同，则该类型可以无序列化地在两个平台之间传输。

本文档对这一假设进行系统性审视。

---

## 1. 正确性分析：签名匹配 ≟ 二进制兼容

### 1.1 签名编码的内容

Layout 签名编码了以下信息：

| 编码内容 | 示例 | 是否完整 |
|----------|------|----------|
| 架构前缀（位宽+端序） | `[64-le]` | ✅ 完整 |
| 类型总大小 | `s:16` | ✅ 完整 |
| 类型总对齐 | `a:4` | ✅ 完整 |
| 字段绝对偏移 | `@0`, `@4`, `@8` | ✅ 完整 |
| 字段类型签名 | `u32[s:4,a:4]` | ✅ 完整 |
| 字段大小和对齐 | `[s:4,a:4]` | ✅ 完整 |
| Padding（隐式） | 偏移间隙 | ⚠️ 隐式推断 |
| vptr 存在性 | `,vptr` | ✅ 完整 |

### 1.2 签名匹配 → 二进制兼容：成立条件

**定理**: 当且仅当以下条件全部满足时，`layout_sig(T@A) == layout_sig(T@B)` 保证 `T` 可在平台 A 和 B 之间无序列化传输：

| # | 条件 | 说明 | TypeLayout 是否验证 |
|---|------|------|---------------------|
| C1 | 相同端序 | 两平台的字节序相同 | ✅ 架构前缀 `[64-le]` 包含端序 |
| C2 | 相同字段偏移 | 编译器对每个字段放置在相同的字节偏移 | ✅ `@offset` 显式编码 |
| C3 | 相同字段大小 | 每个叶字段占用相同字节数 | ✅ `s:SIZE` 显式编码 |
| C4 | 相同总大小 | 含尾部 padding 的总 sizeof | ✅ `s:SIZE` 显式编码 |
| C5 | 相同对齐要求 | 影响数组元素间距和结构体在内存中的放置 | ✅ `a:ALIGN` 显式编码 |
| C6 | 相同基本类型表示 | `uint32_t` 在两平台都是 4 字节无符号整数 | ✅ 签名匹配保证大小相同 |
| C7 | 无 trap representation | 填充字节不影响值读取 | ⚠️ 不验证（C++ 标准未保证） |
| C8 | 浮点表示一致 | 都使用 IEEE 754 | ⚠️ 假设成立（当前主流平台均支持） |

**结论**: 对于**仅使用定宽整数和 IEEE 754 浮点数的 POD 类型**，签名匹配是二进制兼容的**充分条件**。

### 1.3 已知的正确性漏洞（False Positive 场景）

以下场景中，签名可能匹配但数据**不一定**二进制兼容：

#### 漏洞 1: Bit-field 布局

```cpp
struct Flags {
    uint32_t a : 3;
    uint32_t b : 5;
    uint32_t c : 24;
};
```

**问题**: C++ 标准将 bit-field 的分配顺序定义为 *implementation-defined* (C++20 §11.4.9)。
MSVC 和 GCC/Clang 对同一 bit-field 声明可能产生不同的位排列。

**TypeLayout 当前处理**: 签名编码了 bit-field 的字节偏移、位偏移和宽度 (`@0.0:bits<3,...>`)，
但这些值来自**当前编译器**的反射结果。如果两个平台的编译器恰好对 bit-field 使用不同的打包策略
但产生了相同的偏移（unlikely but possible），签名匹配但布局不同。

**实际风险**: **低**。bit-field 已在文档中标记为"非可移植"，且 TypeLayout 的 V1 保证
是单向蕴含（签名相同→布局相同），bit-field 的签名在不同编译器上通常会不同。

**建议**: 在兼容性报告中对含 bit-field 的类型发出显式警告。

#### 漏洞 2: Padding 字节的值

```cpp
struct S { char a; /* 3 bytes padding */ int32_t b; };
```

**问题**: 签名只编码字段偏移和大小，隐式暗示了 padding 的存在（@0 到 @4 之间有 3 字节间隙）。
但 `memcpy` 在不同平台上对 padding 字节的处理不一致——有些编译器会零初始化 padding，
有些不会。

**实际风险**: **极低**。`memcpy`/`memcmp` 操作复制所有字节（含 padding），只要发送方和
接收方不依赖 padding 的特定值，这不影响功能正确性。但如果用户用 `memcmp` 比较两个
"逻辑相同"的结构体实例，padding 差异可能导致误判。

**建议**: 在文档中说明"签名匹配保证布局兼容，不保证 padding 字节的值一致"。

#### 漏洞 3: `int` 的大小

```cpp
struct S { int count; double value; };
```

**问题**: `int` 虽然在当前主流平台上都是 4 字节，但 C++ 标准仅保证 `sizeof(int) >= 2`。
在某些嵌入式平台上，`int` 可能是 2 字节。

**TypeLayout 当前处理**: 签名会编码实际大小 `i32[s:4,a:4]` 或 `i16[s:2,a:2]`，
所以如果 `int` 大小不同，签名自然不匹配。

**实际风险**: **无**。签名系统已正确处理此情况。

#### 漏洞 4: 浮点表示

**问题**: 签名编码 `f32[s:4,a:4]`（IEEE 754 single）和 `f64[s:8,a:8]`（IEEE 754 double）。
如果某平台使用非 IEEE 754 浮点（如 IBM hexadecimal floating-point），签名大小可能
相同但表示不同。

**实际风险**: **几乎为零**。所有目标平台（x86, ARM, RISC-V）均使用 IEEE 754。
z/Architecture (IBM) 在最新硬件上也支持 IEEE 754。

**建议**: 无需修改，但可在文档中注明"假设 IEEE 754 浮点表示"。

### 1.4 正确性总结

| 分类 | 结论 |
|------|------|
| **定宽整数 POD** | ✅ 签名匹配 = 完全二进制兼容 |
| **含 float/double** | ✅ 实际安全（IEEE 754 假设） |
| **含 bit-field** | ⚠️ 签名可能正确反映差异，但需显式警告 |
| **含 padding** | ✅ 布局兼容，但 padding 值不保证 |
| **含平台依赖类型** | ✅ 签名会自然反映差异（long, wchar_t, long double） |
| **含指针** | ✅ 签名反映指针大小差异，但指针值本身不可跨进程传输 |

**核心结论**: V1 保证 (`layout_sig match ⟹ memcmp-compatible`) 在主流平台上**成立**，
前提是类型仅包含标量字段（定宽整数、浮点、枚举）。含指针的类型布局可能匹配，
但指针值本身需要地址空间转换——这超出了布局兼容的范畴。

---

## 2. 架构合理性分析

### 2.1 Two-Phase Pipeline 的合理性

```
Phase 1: P2996 编译器 → .sig.hpp（每平台一次）
Phase 2: C++17 编译器 → 比对结果（任意平台一次）
```

**优点**:

| 优点 | 说明 |
|------|------|
| P2996 隔离 | Phase 2 无需特殊编译器，任意 C++17 编译器可运行 |
| 离线验证 | .sig.hpp 可提交到 Git，CI 无需访问所有目标平台 |
| 编译时保证 | `static_assert` 在构建时失败，不需运行时检查 |
| 可组合性 | 签名文件可独立生成、传输、归档 |
| 增量验证 | 新增平台只需生成新的 .sig.hpp |

**潜在问题**:

| 问题 | 严重程度 | 说明 |
|------|----------|------|
| 签名文件可能过时 | 中 | 如果类型定义修改但忘记重新导出签名 |
| 需要 P2996 编译器 | 低 | P2996 尚未标准化，但已有稳定的 Bloomberg fork |
| 签名只在编译时生成 | — | 无法在运行时为动态加载的类型生成签名 |

**结论**: Two-Phase 设计**高度合理**。P2996 依赖隔离到 Phase 1 是关键的工程决策。

### 2.2 宏声明式 API 的合理性

```cpp
// Phase 1 — 3 行
TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord, SharedMemRegion)

// Phase 2 — 1 行
TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang)
```

**优点**: 极低的入门门槛，用户无需理解 SigExporter 内部机制。
**缺点**: 宏魔法（FOR_EACH）使调试困难；最大类型数限制为 32。

**结论**: 对于目标用户（需要验证 IPC/网络协议兼容性的 C++ 开发者），这是**正确的抽象级别**。

### 2.3 与核心库的解耦

当前架构：

```
Core Library (signature.hpp)  ←  纯编译时，header-only
        ↓ 使用
Tools Layer (sig_export.hpp, compat_check.hpp)  ←  运行时 I/O
        ↓ 使用
CLI + CI (typelayout-compat, compat-check.yml)  ←  外部编排
```

这种分层是合理的：核心库保持纯粹的编译时特性，工具层处理 I/O 和跨平台编排。

---

## 3. 替代方案对比

### 3.1 序列化框架

| 方案 | 保证 | 开销 | 生态 | 与 TypeLayout 互补性 |
|------|------|------|------|---------------------|
| **Protocol Buffers** | 版本兼容，跨语言 | 编解码开销 | 极广 | 非竞争：protobuf 用于需要序列化的场景 |
| **FlatBuffers** | Zero-copy 读取 | 需要 schema + 构建步骤 | 广 | 部分竞争：FlatBuffers 也支持 zero-copy |
| **Cap'n Proto** | Zero-copy RPC | 需要 schema | 中 | 部分竞争 |

**关键区别**: TypeLayout 验证的是 *原生 C++ struct 的二进制兼容性*。
序列化框架定义的是 *独立于语言的数据格式*。

- 如果你能控制两端的 C++ 代码且已有 `struct` 定义 → TypeLayout 更轻量
- 如果你需要跨语言、跨版本兼容 → 序列化框架更合适
- 如果你需要最高性能的同构 C++ 通信 → TypeLayout 提供编译时保证

**结论**: TypeLayout **不是序列化框架的替代品**，而是一个**补充工具**。
它回答的问题是："我已有的 struct 是否恰好可以无序列化传输？"

### 3.2 调试信息方案

| 方案 | 方式 | 优点 | 缺点 |
|------|------|------|------|
| **pahole + DWARF** | 从编译产物提取布局 | 精确到字节，不需源码修改 | 需要 debug 编译，不是编译时验证 |
| **clang -fdump-record-layouts** | 编译器输出 | 最准确 | 不可编程化，输出格式不稳定 |
| **Compiler Explorer (Godbolt)** | 可视化 | 直观 | 手动操作，不可自动化 |

**关键区别**: 这些工具是 *诊断工具*，不是 *验证工具*。
它们告诉你"布局是什么"，但不能自动化地回答"两个平台的布局是否一致"。

**TypeLayout 的独特价值**: 将布局分析嵌入到编译流程中，使其成为一个**可持续的自动化门控**。

### 3.3 手动 static_assert

```cpp
// 传统方式
static_assert(sizeof(PacketHeader) == 16, "unexpected size");
static_assert(offsetof(PacketHeader, magic) == 0, "unexpected offset");
static_assert(offsetof(PacketHeader, version) == 4, "unexpected offset");
// ... 每个字段一行
```

**缺点**: 需要手动列举每个字段的偏移，容易遗漏；修改类型时需要同步更新。
**TypeLayout 优势**: 自动提取所有字段信息，一行代码验证整个类型。

---

## 4. 实际用户故事分析

### US1: 共享内存 IPC

> "我们有一个 Linux x86-64 的数据采集进程和一个 ARM64 的 HMI 显示进程，
> 通过共享内存传递 `SensorRecord`。需要确认两端对 struct 的内存布局相同。"

**可行性分析**:

| 维度 | 分析 |
|------|------|
| 布局兼容 | ✅ 如果 `SensorRecord` 只用定宽整数和 float，且签名匹配，则完全安全 |
| 端序 | ✅ 签名前缀包含端序信息；x86-64 和 ARM64 默认都是 little-endian |
| 对齐 | ✅ 签名编码对齐要求；共享内存通常 page-aligned |
| TypeLayout 价值 | **高** — CI 中自动验证，避免上线后发现布局不一致 |

**TypeLayout 工作流**:
1. Phase 1: 在 x86-64 Linux Docker 和 ARM64 Linux Docker 中分别运行 `TYPELAYOUT_EXPORT_TYPES`
2. Phase 2: 在 CI 中运行 `TYPELAYOUT_ASSERT_COMPAT` — 编译通过即保证兼容

**风险**: 需要确保 Docker 镜像中的编译器版本与生产环境一致。

### US2: 网络协议

> "我们定义了一组 `struct` 作为网络协议头，客户端运行在 Windows (MSVC)，
> 服务端运行在 Linux (GCC)。需要确认协议头在两端布局一致。"

**可行性分析**:

| 维度 | 分析 |
|------|------|
| 布局兼容 | ⚠️ 取决于字段类型。如果使用 `long`，Windows (4B) vs Linux (8B) **不兼容** |
| 端序 | ✅ 都是 little-endian（x86-64） |
| ABI 差异 | ⚠️ MSVC 和 GCC 对同一 struct 可能有不同的 padding 策略（特别是含 `double` 的 packed struct） |
| TypeLayout 价值 | **极高** — 正是这类场景最容易出错，签名自动检测差异 |

**关键建议**: 网络协议 struct 应该**只使用定宽整数** (`uint32_t`, `int64_t` 等)。
TypeLayout 可以在 CI 中自动验证这一约束。

### US3: 文件格式

> "我们的应用在 macOS ARM64 和 Linux x86-64 之间交换二进制文件，
> 文件头是一个固定格式的 `FileHeader` struct。"

**可行性分析**:

| 维度 | 分析 |
|------|------|
| 布局兼容 | ✅ 如果只用定宽整数，签名匹配即安全 |
| 持久性 | ⚠️ 文件格式需要长期稳定；TypeLayout 验证的是"编译时的当前布局"，不保证未来版本 |
| TypeLayout 价值 | **高** — 每次修改类型定义后自动检测是否破坏了文件格式兼容性 |

**建议**: 将 `.sig.hpp` 文件作为"格式版本"提交到 Git。如果签名发生变化，
CI 自动拦截并要求开发者显式确认格式变更。

### US4: 插件系统

> "我们的应用通过动态库加载插件，主程序和插件之间通过 C struct 接口通信。
> 需要确认主程序和插件对 struct 的布局理解一致。"

**可行性分析**:

| 维度 | 分析 |
|------|------|
| 适用性 | ⚠️ 如果主程序和插件使用同一编译器、同一平台，布局自然一致 |
| TypeLayout 价值 | **中** — 主要价值在于检测**不同编译器版本**之间的 ABI 差异 |
| 更好的方案 | 使用 `extern "C"` + 显式版本号可能更实际 |

---

## 5. 改进建议

基于以上分析，建议以下改进（按优先级排序）：

### P1: 文档增强 — 明确声明正确性边界

在 README 和工具文档中显式说明：

```
TypeLayout 保证: 如果两个平台上同一类型的 Layout 签名完全相同，
则该类型的内存布局在两个平台上完全一致（字段偏移、大小、对齐均相同）。

这意味着: 对于仅包含标量字段（定宽整数、浮点、枚举、字符数组）的 POD 类型，
签名匹配等价于可以使用 memcpy 安全传输。

注意事项:
- 假设 IEEE 754 浮点表示（所有主流平台均满足）
- 含指针的类型：布局可能匹配，但指针值不可跨进程传输
- 含 bit-field 的类型：已标记为非可移植，建议避免在跨平台数据中使用
- Padding 字节的值不保证一致，但不影响数据字段的正确性
```

### P2: 兼容性报告增强 — 添加安全等级

在 `CompatReporter` 的输出中为每个类型添加安全等级：

```
Type                    Layout  Safety     Verdict
PacketHeader            MATCH   ★★★ Safe   Serialization-free (fixed-width only)
SensorRecord            MATCH   ★★★ Safe   Serialization-free (fixed-width + float)
UnsafeStruct            DIFFER  ★☆☆ Risk   Needs serialization (platform-dependent types)
PointerStruct           MATCH   ★★☆ Warn   Layout matches but contains pointers
BitFieldStruct          MATCH   ★☆☆ Risk   Layout matches but bit-field ordering is impl-defined
```

### P3: 签名增强 — Padding 显式化（可选）

在 Layout 签名中显式编码 padding 区域：

```
// 当前:  record[s:16,a:8]{@0:char[s:1,a:1],@8:i64[s:8,a:8]}
// 增强:  record[s:16,a:8]{@0:char[s:1,a:1],@1:pad[7],@8:i64[s:8,a:8]}
```

**优先级**: 低。当前的隐式 padding 推断已经足够安全（偏移差 = padding 大小）。

### P4: 类型安全分类 — 自动检测不安全成员

在签名导出阶段，自动检测并标记含以下成员的类型：
- 指针类型 (`ptr`, `fnptr`, `memptr`)
- 平台依赖类型 (`long`, `wchar_t`, `long double`)
- bit-field
- 非 POD 类型（含虚函数表）

---

## 6. 最终结论

### 方案是否正确？

**是的，在明确的前提条件下**。V1 保证在主流平台上成立。
需要在文档中清晰声明前提条件（IEEE 754, 定宽整数）。

### 设计是否合理？

**是的**。Two-Phase Pipeline 是正确的架构选择：
- Phase 1 隔离了 P2996 依赖
- Phase 2 允许任意编译器验证
- 宏声明式 API 大幅降低了使用门槛

### 是否有更好的替代方案？

**没有直接替代品**。TypeLayout 填补了一个独特的生态位：
- 不需要外部 schema（vs protobuf/FlatBuffers）
- 编译时保证（vs pahole/DWARF）
- 自动化（vs 手动 static_assert + offsetof）
- 纯 C++（vs 需要额外工具链的方案）

### 改进方向

1. **必做**: 文档增强，声明正确性边界（P1）
2. **应做**: 兼容性报告增加安全等级（P2）
3. **可选**: Padding 显式化、不安全成员检测（P3, P4）

---

## Open Questions

1. P2 的安全等级分类逻辑是否应该内置于 `sig_types.hpp`（编译时确定），
   还是仅作为 `CompatReporter` 的运行时输出？
2. 是否需要支持 big-endian 平台的实际验证（目前只在签名前缀中标记端序）？
3. 是否需要一个"strict mode"，在检测到不安全成员时将 `ASSERT_COMPAT` 直接 fail？
