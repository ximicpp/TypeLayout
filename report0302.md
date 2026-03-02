```markdown
# TypeLayout 设计与实现修改方案（基于核心价值分析）

> **版本**: v1.0
> **日期**: 2026-03-02
> **范围**: 架构重组、签名模型修正、分层职责清晰化

---

## 一、核心价值回顾

上一轮分析识别出三个不可再分解的原子核心：

| 编号 | 核心 | 本质 | 一句话描述 |
|------|------|------|-----------|
| **Core A** | 编译期类型布局指纹化 | 同态映射：类型布局空间 → 字符串空间 | 把散落的 sizeof/alignof/offset 凝聚为一个原子身份标识 |
| **Core B** | 签名的跨编译上下文传输 | 物化机制：编译期产物 → 可被另一次编译读取的源代码 | 让两次独立编译的签名能相遇、能比较 |
| **Core C** | 平台环境描述 | 类型之外的编译环境参数 | pointer_size、sizeof_long、sizeof_wchar_t、字节序等 |

**所有其他功能（跨平台比较、兼容性报告、static_assert 断言、人类可读输出）都是基于这三个核心的派生应用。**

### 核心价值与派生应用的完整映射

```text
Core A（指纹化）
├── 派生：Layout 签名 —— 扁平化字节身份，用于兼容性判定
├── 派生：Definition 签名 —— 树状结构描述，用于诊断和文档
├── 派生：递归嵌套类型处理 —— Core A 的完备性要求
├── 派生：虚表指针检测 —— Core A 的完备性要求
└── 派生：位域编码 —— Core A 的完备性要求

Core B（传输）
├── 派生：.sig.hpp 导出 —— 当前唯一的传输格式
├── 派生：消费侧不需要 P2996 —— Core B 的设计后果
└── 派生：多平台签名聚合 —— Core B + 比较逻辑

Core C（平台描述）
├── 派生：平台元数据编码 —— PlatformInfo 结构体
└── 派生：平台差异报告 —— Core C + 比较逻辑

派生应用层（基于 Core A + B + C 的组合）
├── 跨编译边界布局不兼容检测 = Core A + Core B + 字符串比较
├── 跨平台布局比较 = Core A + Core B + Core C + 字符串比较
├── 编译期 static_assert 拦截 = Core A + Core B + constexpr 相等判定
├── 运行时兼容性报告 = Core A + Core B + Core C + 字符串解析 + 格式化
└── 人类可读布局描述 = Core A（Definition 模式）的输出格式选择
```

---

## 二、当前设计的问题诊断

### 2.1 问题清单

| # | 问题 | 影响 | 严重度 |
|---|------|------|--------|
| P1 | 签名中混入了平台前缀 `[64-le]`，Core A 与 Core C 耦合 | 同一布局在不同平台上签名不同，导致不必要的不等；比较逻辑被迫先剥离前缀 | **高** |
| P2 | Layout / Definition 两种模式并列，职责边界模糊 | 用户不知道该用哪个做兼容性检测、哪个做诊断 | 中 |
| P3 | 传输层绑死 `.sig.hpp` 一种格式 | 无法嵌入 object section、无法走 JSON/binary 通道 | 低（当前够用） |
| P4 | 应用层直接操作签名字符串，没有结构化中间层 | CompatReporter 做差异诊断时需要重新解析字符串 | 中 |
| P5 | 签名是纯字符串拼接，无分隔符转义 | 理论上成员名包含 `]` `:` `{` 等字符可破坏解析（实际 C++ 标识符不含这些字符，风险极低） | 低 |
| P6 | `introduces_vptr` 的检测依赖 `sizeof` 差值启发式 | 在某些边缘情况下（如空基类优化叠加虚继承）可能误判 | 中 |

### 2.2 问题之间的因果关系

```text
P1（Core A/C 耦合）
 └─→ P4（应用层被迫解析字符串来剥离平台前缀）
      └─→ 诊断困难，代码复杂

P2（模式职责模糊）
 └─→ 用户困惑 → 可能用错模式 → 误报或漏报

P6（vptr 启发式）
 └─→ 签名可信度降低 → Core A 的单射性被破坏
```

**优先级排序：P1 > P6 > P2 > P4 > P3 > P5**

---

## 三、目标架构

### 3.1 当前架构 vs 目标架构

**当前架构（扁平混合）：**

```text
┌───────────────────────────────────────────────────────────┐
│  签名生成 + 平台前缀 + 导出 + 比较 + 报告 + 断言（全混合）  │
└───────────────────────────────────────────────────────────┘
```

**目标架构（分层分离）：**

```text
┌─────────────────────────────────────────────────────────────┐
│ Layer 3: Applications                                       │
│   CompatReporter / ASSERT 宏 / 布局文档生成 / 版本迁移检查    │
├─────────────────────────────────────────────────────────────┤
│ Layer 2: Comparison & Diagnostics                           │
│   结构化签名比较 / 差异定位 / 兼容性判定规则                    │
├─────────────────────────────────────────────────────────────┤
│ Layer 1: Transport                                          │
│   .sig.hpp 导出 / 未来: JSON / binary / object section       │
├─────────────────────────────────────────────────────────────┤
│ Layer 0: Core Primitives                                    │
│   TypeFingerprint(T) / PlatformDescriptor / FixedString     │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 层间依赖规则

```text
Layer 3 → Layer 2 → Layer 1 → Layer 0
             │                    │
             └────────────────────┘
             （Layer 2 也可直接使用 Layer 0）

禁止：Layer 0 依赖任何上层
禁止：Layer 1 依赖 Layer 2 或 Layer 3
```

---

## 四、逐层具体修改

### 4.1 Layer 0：核心原语的修改

#### 修改 M1：将签名拆分为内容签名 + 平台上下文（解决 P1）

**当前设计：**

```cpp
// 签名 = 平台前缀 + 类型布局
// "[64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:i64[s:8,a:8]}"
//  ^^^^^^ 平台     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ 布局
```

**问题示例：**

```cpp
// 一个不含指针的纯 int struct
// 32 位：[32-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}
// 64 位：[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}
// 布局完全相同，但签名不同 → 误报不兼容
```

**修改方案：**

```cpp
// 分离为两个独立概念

// 1. TypeFingerprint —— 纯布局签名，不含平台前缀
template <typename T, SignatureMode Mode>
struct TypeFingerprint {
    static constexpr auto layout_sig = /* 不含 [64-le] 前缀的纯布局字符串 */;
};

// 2. PlatformDescriptor —— 独立的平台描述
struct PlatformDescriptor {
    static constexpr std::size_t pointer_size = sizeof(void*);
    static constexpr bool is_little_endian = TYPELAYOUT_LITTLE_ENDIAN;
    static constexpr std::size_t sizeof_long = sizeof(long);
    static constexpr std::size_t sizeof_wchar_t = sizeof(wchar_t);
    static constexpr std::size_t sizeof_long_double = sizeof(long double);
};
```

**签名生成函数的具体改动：**

```cpp
// 当前代码（signature_impl.hpp 中）：
// static constexpr auto prefix = make_arch_prefix();  // "[64-le]"
// static constexpr auto sig = prefix + body;

// 改为：
// static constexpr auto sig = body;  // 纯布局，无平台前缀

// arch_prefix 移至 PlatformDescriptor 或导出时附加
```

**向后兼容策略：**

```text
1. 保留 make_arch_prefix() 函数，但不再用于签名生成
2. 在 .sig.hpp 导出时，平台前缀作为 PlatformDescriptor 的一部分独立导出
3. 提供一个 full_sig() 函数用于兼容旧代码：
   full_sig = arch_prefix + layout_sig
```

#### 修改 M2：明确两种签名模式的职责（解决 P2）

**当前状态：**

```text
Layout 模式：扁平化，无名字 → 用途不明确
Definition 模式：树状，有名字 → 用途不明确
两者并列，用户不知道选哪个
```

**修改方案：明确命名和职责**

```text
Layout 签名（重命名为 CompatSig）：
  用途：兼容性判定的唯一依据
  特征：扁平化、无成员名、纯字节身份
  使用场景：ASSERT 宏、CompatReporter 的 pass/fail 判定

Definition 签名（重命名为 DiagSig）：
  用途：差异诊断的辅助信息
  特征：树状、保留成员名、保留结构层次
  使用场景：CompatReporter 的差异报告、布局文档生成
```

**代码层面：**

```cpp
// 当前
enum class SignatureMode { Layout, Definition };

// 改为
enum class SignatureMode {
    Compat,     // 兼容性判定用（原 Layout）
    Diagnostic  // 诊断报告用（原 Definition）
};
```

**文档和注释中明确：**

```cpp
/// CompatSig 是兼容性判定的唯一权威：
///   如果 CompatSig 相同 → 布局兼容，可以安全 reinterpret
///   如果 CompatSig 不同 → 布局不兼容
///
/// DiagSig 永远不用于兼容性判定，仅用于：
///   告诉用户"哪里不同"、"为什么不同"
```

#### 修改 M3：加固 vptr 检测（解决 P6）

**当前启发式：**

```cpp
template <typename T>
consteval bool introduces_vptr() {
    // 检查 T 是否多态
    // 如果多态且无多态基类 → T 引入了 vptr
    // 通过 sizeof 差值推断 vptr 位置
}
```

**问题：**

```text
1. 空基类优化 (EBO) 可能导致 sizeof 差值不等于 pointer_size
2. 虚继承场景下，vptr 和 vbase offset 指针可能共存
3. 不同编译器的 vptr 策略不同（MSVC vs Itanium ABI）
```

**修改方案：**

```cpp
// 方案 A：保留启发式，但增加校验和文档
template <typename T>
consteval bool introduces_vptr() {
    if constexpr (!std::is_polymorphic_v<T>) {
        return false;
    }
    // 检查是否有多态基类
    bool has_polymorphic_base = /* 遍历基类列表 */;
    if (!has_polymorphic_base) {
        return true;  // T 是多态继承链的起点
    }
    // 如果有多态基类但 sizeof(T) > 预期大小
    // → 可能引入了额外的 vptr（多重继承场景）
    // → 标记为 uncertain，在签名中用 vptr? 表示
    return /* 启发式判断 */;
}

// 方案 B（更保守）：只标记"多态/非多态"，不推断 vptr 位置
// vptr 的存在通过 sizeof 隐式反映
// 签名中只标记 [polymorphic] 而不插入合成的 vptr 字段
```

**推荐方案 B，理由：**

```text
1. vptr 的布局细节是 ABI 特定的，试图精确编码它违反了"签名应跨 ABI 可比较"的原则
2. vptr 导致的 sizeof 变化已经通过 sizeof 编码在签名中了
3. 插入合成 vptr 字段可能与真实的成员 offset 冲突
4. 标记 [polymorphic] 足以提醒用户"这个类型含虚函数，跨 ABI 使用需注意"
```

---

### 4.2 Layer 1：传输层的修改

#### 修改 M4：将导出格式与签名生成解耦（解决 P3）

**当前设计：**

```text
sigexport.hpp 直接生成 .sig.hpp 的 C++ 源代码字符串
签名生成和格式化绑定在一起
```

**修改方案：引入中间表示**

```cpp
// 中间表示：与输出格式无关的结构化数据
struct ExportData {
    // 类型签名列表
    struct TypeEntry {
        std::string_view type_name;
        std::string_view compat_sig;
        std::string_view diag_sig;
    };
    std::vector<TypeEntry> types;

    // 平台描述
    PlatformDescriptor platform;

    // 元数据
    std::string_view generator_version;
    std::string_view timestamp;
};

// 格式化器接口（编译期）
// 格式 1：.sig.hpp（当前格式，保持兼容）
template <typename... Types>
constexpr auto export_as_hpp();

// 格式 2：未来扩展
// template <typename... Types>
// constexpr auto export_as_json();
```

**当前阶段只保留 `.sig.hpp` 格式，但架构上做好扩展准备。不过度设计。**

#### 修改 M5：.sig.hpp 文件结构调整

**当前 .sig.hpp 结构：**

```cpp
// 平台信息和签名混在一起
namespace typelayout::sigs::platform_x {
    constexpr auto layout_sig = "[64-le]record...";
    constexpr auto definition_sig = "[64-le]Foo{...}";
    // PlatformInfo 嵌在里面
}
```

**修改后的 .sig.hpp 结构：**

```cpp
// 平台描述独立区域
namespace typelayout::sigs::platform_x {

    // 区域 1：平台描述（Core C）
    struct Platform {
        static constexpr std::size_t pointer_size = 8;
        static constexpr bool is_little_endian = true;
        static constexpr std::size_t sizeof_long = 4;
        static constexpr std::size_t sizeof_wchar_t = 2;
        static constexpr std::size_t sizeof_long_double = 8;
    };

    // 区域 2：类型签名（Core A）—— 不含平台前缀
    namespace types {
        struct Foo {
            static constexpr auto compat_sig =
                "record[s:16,a:8]{@0:i32[s:4,a:4],@8:i64[s:8,a:8]}";
            static constexpr auto diag_sig =
                "Foo{x:i32[s:4,a:4],y:i64[s:8,a:8]}[s:16,a:8]";
        };
    }
}
```

---

### 4.3 Layer 2：比较与诊断层（新增）

#### 修改 M6：引入结构化比较（解决 P4）

**当前方式：**

```text
签名 A == 签名 B？
  相等 → 兼容
  不等 → 不兼容
    → 然后用字符串操作来"猜"哪里不同
```

**修改方案：两阶段比较**

```text
阶段 1：快速判定（compat_sig 字符串相等）
  相等 → 兼容，结束
  不等 → 进入阶段 2

阶段 2：结构化差异分析（使用 diag_sig）
  解析 diag_sig → 结构化表示
  逐成员比较 → 生成差异列表
  输出：哪个成员在哪个 offset 不匹配，size/align 差了多少
```

**结构化差异输出示例：**

```text
Type 'Foo' layout mismatch:
  Platform A (x86_64-linux-gcc13) vs Platform B (x86_64-windows-msvc17)

  Member 'data':
    A: offset=8, size=8, align=8, type=long
    B: offset=8, size=4, align=4, type=long
    Cause: sizeof(long) differs (8 vs 4)

  Overall:
    A: sizeof=16, alignof=8
    B: sizeof=12, alignof=4
```

**实现策略：**

```cpp
// 比较结果的结构化表示
struct FieldDiff {
    std::string_view field_name;
    std::size_t offset_a, offset_b;
    std::size_t size_a, size_b;
    std::size_t align_a, align_b;
    std::string_view type_a, type_b;
};

struct CompareResult {
    bool compatible;                 // compat_sig 是否相等
    std::string_view type_name;
    std::vector<FieldDiff> diffs;    // 仅当 !compatible 时填充
    // 平台信息
    PlatformDescriptor platform_a;
    PlatformDescriptor platform_b;
};

// 比较函数
CompareResult compare_types(
    std::string_view compat_sig_a, std::string_view diag_sig_a,
    std::string_view compat_sig_b, std::string_view diag_sig_b,
    const PlatformDescriptor& platform_a,
    const PlatformDescriptor& platform_b
);
```

---

### 4.4 Layer 3：应用层的修改

#### 修改 M7：CompatReporter 使用结构化比较

**当前 CompatReporter：**

```text
直接拿签名字符串 → 相等判断 → 手动拼报告
```

**修改后的 CompatReporter：**

```cpp
class CompatReporter {
public:
    // 添加类型比较（使用 Layer 2 的 CompareResult）
    template <typename PlatformA, typename PlatformB>
    void add_type(std::string_view type_name) {
        auto result = compare_types(
            PlatformA::types::compat_sig,
            PlatformA::types::diag_sig,
            PlatformB::types::compat_sig,
            PlatformB::types::diag_sig,
            PlatformA::Platform{},
            PlatformB::Platform{}
        );
        results_.push_back(result);
    }

    // 生成报告
    std::string generate_report() const;

    // 是否全部兼容
    bool all_compatible() const;
};
```

#### 修改 M8：ASSERT 宏简化

**当前：**

```cpp
#define TYPELAYOUT_ASSERT_COMPAT(TypeA_sig, TypeB_sig) \
    static_assert(TypeA_sig == TypeB_sig, "Layout mismatch")
```

**修改后：**

```cpp
// 只比较 compat_sig，不比较 diag_sig
// 不比较平台前缀（因为已经分离了）
#define TYPELAYOUT_ASSERT_LAYOUT_COMPAT(PlatformA, PlatformB, TypeName) \
    static_assert(                                                       \
        ::typelayout::detail::fixed_string_equal(                        \
            PlatformA::types::TypeName::compat_sig,                      \
            PlatformB::types::TypeName::compat_sig                       \
        ),                                                               \
        "Layout of " #TypeName " is incompatible between platforms"      \
    )
```

---

## 五、文件结构重组

### 5.1 当前文件结构

```text
boost/typelayout/
├── config.hpp
├── fixed_string.hpp
├── typelayout.hpp
├── detail/
│   ├── reflect.hpp
│   ├── signature_impl.hpp
│   ├── type_map.hpp
│   └── vptr_detect.hpp
├── signature.hpp
├── sigexport.hpp
├── compat_assert.hpp
└── compat_reporter.hpp
```

### 5.2 目标文件结构

```text
boost/typelayout/
├── config.hpp                      # 平台检测宏（不变）
├── fixed_string.hpp                # FixedString 工具（不变）
├── typelayout.hpp                  # 总入口（不变）
│
├── core/                           # Layer 0: 核心原语
│   ├── fingerprint.hpp             # TypeFingerprint<T> —— 纯布局签名
│   ├── platform.hpp                # PlatformDescriptor —— 平台描述
│   └── detail/
│       ├── reflect.hpp             # P2996 反射操作（不变）
│       ├── signature_impl.hpp      # 签名生成实现（移除平台前缀）
│       ├── type_map.hpp            # 类型名映射（不变）
│       └── vptr_detect.hpp         # vptr 检测（简化为多态标记）
│
├── transport/                      # Layer 1: 传输层
│   └── sigexport.hpp               # .sig.hpp 导出（使用新的分离格式）
│
├── compare/                        # Layer 2: 比较与诊断（新增）
│   ├── compare.hpp                 # 结构化比较函数
│   ├── diff.hpp                    # 差异数据结构
│   └── diag_parser.hpp             # DiagSig 解析器
│
└── app/                            # Layer 3: 应用层
    ├── compat_assert.hpp           # ASSERT 宏
    └── compat_reporter.hpp         # 兼容性报告器
```

---

## 六、实施路线图

### 6.1 阶段划分

```text
Phase 1: 签名分离（解决 P1，最高优先级）
  ├── 从签名中移除平台前缀
  ├── 创建独立的 PlatformDescriptor
  ├── 修改 .sig.hpp 导出格式
  ├── 更新 ASSERT 宏和 CompatReporter
  └── 预计工作量：2-3 天

Phase 2: 模式职责明确化（解决 P2）
  ├── 重命名 Layout → Compat, Definition → Diagnostic
  ├── 更新所有引用点
  ├── 添加文档说明
  └── 预计工作量：1 天

Phase 3: vptr 检测简化（解决 P6）
  ├── 将合成 vptr 字段改为 [polymorphic] 标记
  ├── 更新签名格式
  ├── 更新测试用例
  └── 预计工作量：1-2 天

Phase 4: 结构化比较层（解决 P4）
  ├── 实现 DiagSig 解析器
  ├── 实现结构化比较函数
  ├── 重构 CompatReporter 使用新接口
  └── 预计工作量：3-4 天

Phase 5: 文件结构重组
  ├── 按目标结构移动文件
  ├── 更新 include 路径
  ├── 确保向后兼容的 include 转发
  └── 预计工作量：1 天
```

### 6.2 各阶段的依赖关系

```text
Phase 1 ──→ Phase 2 ──→ Phase 5
   │                       ↑
   └──→ Phase 3 ───────────┘
   │
   └──→ Phase 4 ───────────┘
```

**Phase 1 是所有后续阶段的前提。Phase 2/3/4 可以并行。Phase 5 在所有功能完成后执行。**

### 6.3 向后兼容策略

```text
1. 旧的签名格式（含平台前缀）通过 full_sig() 函数保留
2. 旧的 SignatureMode::Layout / Definition 枚举值保留为别名
3. 旧的 .sig.hpp 文件仍可被新代码读取（兼容解析器）
4. 旧的 include 路径通过转发头文件保持可用
5. 所有废弃 API 标记 [[deprecated]] 并提供迁移指引
```

---
由于文档很长，我将从 **第七节（关键设计决策记录）** 的 D3 开始续写到文档结尾，确保完整。如果你需要从头输出，请告知。

```markdown
### 7.3 决策 D3：比较层是编译期还是运行时

**背景**: 签名比较可以发生在编译期（static_assert）或运行时（加载时检查），两者有不同的权衡。

**选项**:

| 选项 | 描述 | 优点 | 缺点 |
|------|------|------|------|
| A | 仅编译期 | 零运行时开销；错误在最早阶段暴露 | 无法检测动态加载的插件；两侧必须在同一构建系统中 |
| B | 仅运行时 | 可检测动态加载场景；可输出丰富诊断 | 问题推迟到运行时才暴露；需要额外的运行时基础设施 |
| C | 编译期为主，运行时为辅 | 兼得两者优势；能覆盖静态链接和动态加载两种场景 | 实现复杂度稍高 |

**决策**: 选择 **C（编译期为主，运行时为辅）**

**理由**:

1. 静态链接场景（最常见）应在编译期就拦截不兼容，零成本
2. 动态加载场景（插件系统）无法在编译期跨越，必须有运行时检查
3. 运行时检查复用编译期生成的签名字符串，不需要额外的元数据格式

**实现要点**:

```cpp
// 编译期：static_assert（Layer 3 应用）
// 利用 Layer 2 的 constexpr 比较函数
static_assert(
    typelayout::layout_compatible<MyStruct, imported::MyStruct_sig>(),
    "Layout mismatch detected"
);

// 运行时：加载时检查（Layer 3 应用）
// 利用 Layer 2 的运行时比较 + 诊断函数
bool ok = typelayout::runtime_check<MyStruct>(
    imported_signature_string,
    typelayout::on_mismatch::log_and_abort
);
```

---

### 7.4 决策 D4：签名格式的稳定性承诺

**背景**: 签名字符串的格式一旦发布，消费侧的代码就会依赖它。格式变更会破坏跨版本兼容。

**选项**:

| 选项 | 描述 |
|------|------|
| A | 签名格式不做稳定性承诺，每次发布可能变更 |
| B | 签名格式做语义化版本承诺，major 版本内稳定 |
| C | 签名格式做永久稳定承诺，只追加不修改 |

**决策**: 选择 **B（语义化版本承诺）**

**理由**:

1. 永久稳定承诺过于激进，会阻碍修复签名中发现的语义缺陷（如 P6 vptr 误判）
2. 不做任何承诺则消费侧无法信赖签名，失去跨时间比较的能力
3. 在 `.sig.hpp` 中嵌入签名格式版本号，消费侧可据此判断是否兼容

**实现要点**:

```cpp
// .sig.hpp 中嵌入格式版本
// 格式版本独立于库版本
namespace typelayout::exported {
    inline constexpr int signature_format_version = 1;

    // 签名内容
    inline constexpr auto MyStruct_layout = typelayout::fixed_string("...");
}
```

---

### 7.5 决策 D5：Definition 签名的定位

**背景**: 当前 Layout 和 Definition 两种签名模式并列，用户不清楚各自用途。

**选项**:

| 选项 | 描述 |
|------|------|
| A | 废弃 Definition 模式，只保留 Layout |
| B | 保留两者，明确 Layout 用于判定、Definition 用于诊断 |
| C | 合并为一种签名，同时包含判定和诊断信息 |

**决策**: 选择 **B（保留两者，明确职责分工）**

**理由**:

1. Layout 签名是最小化的字节级指纹，适合做相等性判定（快速、紧凑）
2. Definition 签名携带字段名、类型名等语义信息，适合做差异诊断（可读、详细）
3. 合并会导致判定签名过大，影响编译期字符串操作的性能
4. 两者的生成逻辑大量共享（递归遍历成员），只是输出格式不同，维护成本可控

**实现要点**:

```text
Layer 0 提供两个函数：
  type_fingerprint<T>()    → 返回 Layout 签名（用于判定）
  type_definition<T>()     → 返回 Definition 签名（用于诊断）

Layer 2 的比较逻辑：
  compatible()             → 仅使用 Layout 签名
  diagnose_difference()    → 同时使用两种签名：Layout 定位差异位置，Definition 输出可读描述

Layer 3 的应用：
  ASSERT 宏               → 使用 Layout 签名做 static_assert
  CompatReporter           → 使用 Definition 签名生成报告
  .sig.hpp 导出            → 同时导出两种签名
```

---

## 八、实施路线图

### 8.1 阶段划分

```text
Phase 1: 签名分离（解决 P1）                         预计工作量: 中
─────────────────────────────────────────────────────────────────
  1a. 将平台前缀从签名字符串中移除
  1b. 创建独立的 PlatformDescriptor 结构体
  1c. 修改 .sig.hpp 导出格式，分别导出布局签名和平台描述
  1d. 更新所有 static_assert 和比较逻辑
  1e. 更新测试用例

Phase 2: 职责明确化（解决 P2）                       预计工作量: 小
─────────────────────────────────────────────────────────────────
  2a. 重命名 API：type_fingerprint / type_definition
  2b. 文档中明确两种签名的用途
  2c. ASSERT 宏默认使用 Layout 签名
  2d. CompatReporter 默认使用 Definition 签名

Phase 3: 比较层结构化（解决 P4）                     预计工作量: 中
─────────────────────────────────────────────────────────────────
  3a. 定义 ComparisonResult 结构体
  3b. 实现 constexpr compare 函数，返回结构化结果
  3c. CompatReporter 改为消费 ComparisonResult
  3d. 减少运行时字符串解析

Phase 4: vptr 检测加固（解决 P6）                    预计工作量: 小
─────────────────────────────────────────────────────────────────
  4a. 增加边缘情况的测试用例
  4b. 记录已知的误判场景和规避方法
  4c. 如有可能，利用 P2996 反射查询 virtual 方法来辅助判定

Phase 5: 传输层可扩展性（解决 P3，可选）              预计工作量: 大
─────────────────────────────────────────────────────────────────
  5a. 抽象传输接口
  5b. 实现 JSON 格式导出（可选）
  5c. 实现 object section 嵌入（可选）
```

### 8.2 阶段依赖关系

```text
Phase 1 ──→ Phase 2 ──→ Phase 3
                │
                └──→ Phase 4（可并行）

Phase 5 依赖 Phase 1 完成，但可独立于 Phase 2/3/4
```

### 8.3 风险评估

| 阶段 | 主要风险 | 缓解措施 |
|------|----------|----------|
| Phase 1 | 签名格式变更导致已生成的 .sig.hpp 失效 | 提供迁移脚本；在新格式中嵌入版本号 |
| Phase 2 | API 重命名导致用户代码编译失败 | 保留旧名称作为 deprecated alias |
| Phase 3 | constexpr 结构化比较可能超出编译器递归限制 | 分层比较；先比较顶层 size/align，不等则提前返回 |
| Phase 4 | P2996 反射 API 可能在不同编译器上行为不一致 | 保留启发式作为 fallback |
| Phase 5 | 过度设计传输层，增加不必要的复杂度 | 标记为可选；仅在有明确需求时实施 |

---

## 九、成功标准

实施完成后，以下条件应全部满足：

| # | 标准 | 验证方法 |
|---|------|----------|
| S1 | 同一类型在不同平台上，如果字节布局碰巧相同，则 Layout 签名相同 | 编写跨平台测试，对比纯 int 结构体的签名 |
| S2 | Layout 签名不同 ⟹ 字节布局确实不同（单射性） | 数学论证 + 穷举小型结构体测试 |
| S3 | 用户能在不查阅源码的情况下，从 API 命名和文档中判断出该用 fingerprint 还是 definition | 用户测试 / 文档审查 |
| S4 | CompatReporter 不需要手动解析签名字符串 | 代码审查：Reporter 只消费 ComparisonResult |
| S5 | .sig.hpp 中包含格式版本号，消费侧可据此判断兼容性 | 检查生成的 .sig.hpp 文件 |
| S6 | 所有现有测试用例在修改后继续通过 | CI 全绿 |

---

## 十、附录

### 附录 A：术语表

| 术语 | 定义 |
|------|------|
| **Layout 签名** | 类型的扁平化字节级指纹，仅包含 size、alignment、offset 和基本类型标记，不含字段名和类型名。用于兼容性判定。 |
| **Definition 签名** | 类型的树状结构描述，包含字段名、类型名、嵌套结构。用于诊断和文档生成。 |
| **PlatformDescriptor** | 描述编译环境的参数集合：pointer_size、sizeof_long、sizeof_wchar_t、endianness 等。独立于具体类型。 |
| **TypeFingerprint** | Layer 0 的核心输出结构，包含 Layout 签名和（可选的）Definition 签名。 |
| **ComparisonResult** | Layer 2 的结构化比较结果，包含是否兼容、差异位置、差异描述等字段。 |
| **Transport** | Layer 1 的传输机制，负责将签名从一个编译上下文传递到另一个编译上下文。当前实现为 .sig.hpp 文件。 |
| **单射性** | 数学性质：不同的输入映射到不同的输出。在此上下文中指"不同的字节布局必须生成不同的签名"。 |
| **同态映射** | 保持结构的映射。在此上下文中指"布局相同 ⟺ 签名相同"。 |

### 附录 B：签名格式变更对照（Phase 1 前后）

**变更前**:

```text
签名字符串 = 平台前缀 + 布局描述
示例: "[64-le]record[s:16,a:8]{@0:i32[s:4,a:4],@8:i64[s:8,a:8]}"
```

**变更后**:

```text
布局签名 = 纯布局描述（不含平台前缀）
示例: "record[s:16,a:8]{@0:i32[s:4,a:4],@8:i64[s:8,a:8]}"

平台描述 = 独立的结构体
示例: PlatformDescriptor{ pointer_size=8, endian=little, sizeof_long=8, sizeof_wchar_t=4 }
```

**迁移方法**:

```text
1. 旧格式签名以 "[" 开头 → 新格式不以 "[" 开头
2. 消费侧可通过首字符判断格式版本
3. 提供 migrate_signature() 工具函数，自动剥离平台前缀
```

### 附录 C：文件结构变更对照（目标状态）

```text
include/typelayout/
├── core/                          # Layer 0: 核心原语
│   ├── fixed_string.hpp           # 编译期字符串基础设施
│   ├── type_fingerprint.hpp       # Core A: 布局指纹生成
│   ├── type_definition.hpp        # Core A: 结构定义生成
│   └── platform_descriptor.hpp    # Core C: 平台环境描述
├── transport/                     # Layer 1: 传输层
│   ├── sig_hpp_exporter.hpp       # .sig.hpp 导出器
│   └── transport_format.hpp       # 传输格式抽象（预留）
├── compare/                       # Layer 2: 比较与诊断
│   ├── comparison_result.hpp      # 结构化比较结果
│   ├── layout_compare.hpp         # 布局签名比较（constexpr）
│   └── platform_compare.hpp       # 平台描述比较
├── app/                           # Layer 3: 应用
│   ├── assert.hpp                 # ASSERT 宏
│   ├── compat_reporter.hpp        # 兼容性报告生成器
│   └── doc_generator.hpp          # 布局文档生成（可选）
└── typelayout.hpp                 # 总入口头文件
```
```

以上从 **7.3 决策 D3** 一直到 **附录 C（文件结构变更对照）** 为文档的完整结尾。如果需要我从第一节开始重新输出完整的单一文档，请告知。