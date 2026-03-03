# TypeLayout 逐层分析：Core 与 Tools

---

## 第一部分：Core 是否需要调整？

### 1.1 当前 Core 清单

```text
boost/typelayout/
├── config.hpp                     平台检测
├── fwd.hpp                        前向声明
├── fixed_string.hpp               编译期字符串
├── signature.hpp                  公共 API
├── layout_traits.hpp              类型元信息
├── opaque.hpp                     不透明类型注册
├── typelayout.hpp                 Umbrella header
└── detail/
    ├── reflect.hpp                P2996 反射封装
    ├── signature_impl.hpp         签名拼接引擎
    ├── type_map.hpp               原始类型映射
    └── hash.hpp                   ← 遗留，应删除
```

### 1.2 逐文件审查

#### ✅ 无需调整的文件（7个）

| 文件 | 理由 |
|------|------|
| `config.hpp` | 职责单一（字节序检测），无多余内容 |
| `fwd.hpp` | 前向声明 + `always_false`，标准做法 |
| `fixed_string.hpp` | 核心基础设施，被签名系统依赖，无可替代 |
| `detail/reflect.hpp` | P2996 封装，TypeLayout 的技术壁垒所在 |
| `detail/signature_impl.hpp` | 签名拼接引擎，Core 的核心算法 |
| `detail/type_map.hpp` | 原始类型到签名标签的映射，签名系统必须 |
| `signature.hpp` | 唯一的公共 API 出口，干净 |

#### ⚠️ 需要小幅审视的文件（2个）

**`layout_traits.hpp`**

```text
当前提供：
  layout_traits<T>::signature       — 签名字符串
  layout_traits<T>::size            — sizeof
  layout_traits<T>::alignment       — alignof
  layout_traits<T>::is_trivially_copyable
  layout_traits<T>::has_pointer
  layout_traits<T>::field_count

问题：
  signature 是核心功能 — ✅ 属于 Core
  size / alignment / field_count — 签名的"原材料"，签名中已编码 — ✅ 属于 Core
  is_trivially_copyable — 签名本身不包含这个信息，
                          它是为 serialization_free 判定服务的
  has_pointer           — 同上

  这两个 trait 的存在是因为上层（Tools）需要它们。
  但它们本身也是类型的客观元信息，放在 Core 并不"错"。

结论：保留，不调整。
  理由：is_trivially_copyable 和 has_pointer 是类型的客观属性，
        Core 提供"类型的元信息"是合理职责。
        即使没有 Tools 层，用户也可能想查询这些信息。
```

**`opaque.hpp`**

```text
当前提供：
  BOOST_TYPELAYOUT_OPAQUE(Type, Tag)  — 注册不透明类型

问题：
  这是签名系统的扩展点，允许用户把不可反射的类型（如 std::string）
  纳入签名体系。

  功能上属于 Core — ✅
  
  但需要确认：是否还残留对 hash.hpp 的依赖？
  上一轮决定 opaque 改用字符串 Tag 而非 OPAQUE_ID/hash。
  需要验证代码是否已完成迁移。

结论：保留，确认无 hash 残留即可。
```

#### ❌ 需要删除的文件（1个）

**`detail/hash.hpp`**

```text
上轮决策：移除
当前状态：文件仍存在

删除理由：
  1. 与"只使用签名字符串比较"的架构决策矛盾
  2. Core 中无文件 #include 它（已解耦）
  3. opaque.hpp 已改用字符串 Tag
  4. 留着会造成 API 表面污染和认知混乱
  5. 如果 Tools 中有引用，也应切断

行动：直接删除。
```

### 1.3 Core 结论

```text
┌────────────────────────────────────────────┐
│  Core 整体判定：健康，仅需 1 项调整        │
│                                            │
│  ✅ 保留不动：8 个文件                      │
│  ❌ 删除：detail/hash.hpp（1个文件）        │
│                                            │
│  调整后 Core = 9 个文件                     │
│  职责单一：T → 编译期布局签名               │
│  内聚度：高                                 │
│  依赖方向：线性，无循环                     │
└────────────────────────────────────────────┘
```

---

## 第二部分：Tools 是否需要调整？

### 2.1 当前 Tools 清单

```text
boost/typelayout/tools/
├── serialization_free.hpp         memcpy 安全判定
├── classify.hpp                   五级安全分类
├── classify_safety.hpp            三级分类（"向后兼容"）
├── compat_check.hpp               兼容性报告
├── compat_auto.hpp                便捷宏
├── sig_export.hpp                 .sig.hpp 导出
├── sig_types.hpp                  TypeEntry / PlatformInfo
└── detail/
    └── foreach.hpp                预处理器 FOR_EACH
```

### 2.2 逐文件审查

---

#### ① `serialization_free.hpp` — ⚠️ 保留，但需明确定位

```text
提供：
  is_local_serialization_free_v<T>           — 编译期，本端判定
  is_serialization_free(local_sig, remote_sig) — 运行时，双端比较

为什么保留：
  这是 TypeLayout 最直接、最高频的使用场景。
  用户拿到签名后，第一个问题就是"能不能 memcpy？"
  如果不提供这个工具，99% 的用户都会自己写一个一模一样的。

为什么是 Tools 而非 Core：
  它包含了策略判定（"什么条件算安全"），
  Core 只说"布局是什么"，不说"该怎么用"。

需要调整的地方：
  检查是否依赖 hash.hpp — 如果有，切换为签名字符串直接比较。

结论：✅ 保留，确认无 hash 依赖。
```

---

#### ② `classify.hpp` — ⚠️ 保留，需精简

```text
提供：
  五级 SafetyLevel：
    TrivialSafe / PaddingRisk / PointerRisk / PlatformVariant / Opaque

为什么保留：
  给用户提供了一个快速了解类型特性的"分诊台"。
  对于不熟悉底层布局的用户，这是有用的教育工具。

为什么是 Tools：
  分类标准是主观策略选择。
  不同应用场景可能定义不同的"安全"含义。

需要检查：
  1. 五个级别是否互斥且穷举？
  2. 分类逻辑是否只依赖 Core 的 layout_traits？
  3. 是否有对 hash.hpp 的残留依赖？

结论：✅ 保留，确认依赖关系干净。
```

---

#### ③ `classify_safety.hpp` — ❌ 删除

```text
提供：
  三级 SafetyLevel：Safe / Warning / Risk
  自称 "backward compatibility wrapper"

为什么删除：

  问题 1 — 冗余
    classify.hpp 已经提供五级分类。
    classify_safety.hpp 只是把五级映射成三级。
    这是一行 switch-case，不值得一个独立文件。

  问题 2 — "向后兼容"不成立
    TypeLayout 尚未发布 1.0，没有用户需要兼容。
    过早引入兼容层是纯负担。

  问题 3 — 交叉依赖
    classify_safety.hpp #include compat_check.hpp
    compat_check.hpp 也定义了 compat::SafetyLevel（三级）
    两个地方定义了语义相同的三级分类 — 违反 DRY 原则。
    形成了 Tools 内部的不必要交叉依赖。

  问题 4 — 概念膨胀
    用户面对三个分类系统：五级、三级（classify_safety）、三级（compat）
    "我到底该用哪个？"— 认知负担。

如果用户需要三级分类：
  在 classify.hpp 中加一个 to_simple_level() 内联函数即可。

结论：❌ 删除整个文件。如需三级映射，合入 classify.hpp 一个函数。
```

---

#### ④ `compat_check.hpp` — ⚠️ 保留，需重构

```text
提供：
  运行时签名兼容性报告
  compat::SafetyLevel（三级）
  逐类型的差异分析

为什么保留：
  "两端布局是否兼容？哪里不同？"
  是 TypeLayout 的核心使用场景之一。
  运行时的签名比较 + 差异报告是有真实价值的。

需要调整：

  1. SafetyLevel 重复定义
     compat_check.hpp 内部定义了自己的三级 SafetyLevel。
     classify_safety.hpp 也定义了三级 SafetyLevel。
     删除 classify_safety.hpp 后，这里的定义变成唯一权威 — 问题消除。

  2. 检查是否依赖 hash.hpp
     如果兼容性检查用的是 hash 比较 — 切换为签名字符串比较。

  3. 检查是否过度设计
     报告结构是否过于复杂？
     对于 v1.0，一个简单的 "match/mismatch + diff string" 可能就够了。

结论：✅ 保留，删除 classify_safety 后自然解决重复问题，
      检查并简化报告结构。
```

---

#### ⑤ `compat_auto.hpp` — ⚠️ 谨慎保留或降级为示例

```text
提供：
  便捷宏，自动注册类型列表进行兼容性检查

为什么存在价值可疑：
  1. 宏是"框架级"便利 — TypeLayout 定位是 library 不是 framework
  2. 依赖 foreach.hpp 的预处理器魔法 — 增加维护复杂度
  3. 用户的使用模式千差万别，一个通用宏很难适配所有场景
  4. 如果用户需要，基于 compat_check.hpp 手写几行即可

两种处理方式：

  方式 A — 保留在 Tools 中，但标记为 "convenience, optional"
    优点：已经写好了，删除浪费
    缺点：维护负担，宏容易出问题

  方式 B — 移到 examples/ 或 recipes/ 目录
    优点：不污染库的公共 API
    缺点：需要重新组织目录

建议：方式 B — 移到 examples/。
  理由：宏不应该出现在库的头文件中。
        作为示例代码比作为 API 更合适。
```

---

#### ⑥ `sig_export.hpp` — ⚠️ 保留，但需确认必要性

```text
提供：
  将编译期签名导出为 .sig.hpp 文件
  用于跨编译单元 / 跨进程的签名传递

为什么保留：
  在 serialization-free 握手场景中，端 A 需要把自己的签名
  "发送"给端 B。.sig.hpp 是一种实现方式。

需要审视：
  1. 是否与 compat_check.hpp 功能重叠？
     compat_check 已经可以加载和比较签名。
     sig_export 是"生成端"，compat_check 是"消费端"。
     两者配合使用 — 不重叠。

  2. 是否依赖 sig_types.hpp？
     是的 — TypeEntry / PlatformInfo 是导出格式的数据结构。
     两者捆绑使用。

  3. 实际使用频率？
     这是构建系统集成的工具。
     大多数用户可能不需要。
     但需要的用户会非常需要。

结论：✅ 保留。与 sig_types.hpp 一起作为"导出工具包"。
```

---

#### ⑦ `sig_types.hpp` — ✅ 保留

```text
提供：TypeEntry / PlatformInfo 等数据结构

与 sig_export.hpp 配套使用。
如果保留 sig_export，就必须保留 sig_types。

结论：✅ 保留。
```

---

#### ⑧ `detail/foreach.hpp` — ⚠️ 随 compat_auto 决定

```text
提供：预处理器 FOR_EACH 宏

唯一用户：compat_auto.hpp

如果 compat_auto 移到 examples/：
  foreach.hpp 也应一起移走。
  
如果 compat_auto 保留在 tools/：
  foreach.hpp 保留在 tools/detail/。

结论：跟随 compat_auto.hpp 的决策。
```

---

### 2.3 Tools 调整汇总

```text
┌──────────────────────────────────────────────────────┐
│  Tools 调整方案                                       │
│                                                      │
│  ✅ 保留不动：                                        │
│    serialization_free.hpp   — 核心使用场景             │
│    classify.hpp             — 类型分诊                 │
│    compat_check.hpp         — 兼容性报告（需小幅简化）  │
│    sig_export.hpp           — 签名导出                 │
│    sig_types.hpp            — 导出数据结构              │
│                                                      │
│  ❌ 删除：                                            │
│    classify_safety.hpp      — 冗余，过早兼容层          │
│                                                      │
│  📦 移到 examples/：                                   │
│    compat_auto.hpp          — 宏不适合做库 API          │
│    detail/foreach.hpp       — 跟随 compat_auto         │
│                                                      │
│  调整后 Tools = 5 个文件                               │
└──────────────────────────────────────────────────────┘
```

---

## 三、调整后全局架构

```text
boost/typelayout/
├── config.hpp                          ┐
├── fwd.hpp                             │
├── fixed_string.hpp                    │
├── signature.hpp                       │  Core（9个文件）
├── layout_traits.hpp                   │  纯编译期
├── opaque.hpp                          │  零策略
├── typelayout.hpp                      │
└── detail/                             │
    ├── reflect.hpp                     │
    ├── signature_impl.hpp              │
    └── type_map.hpp                    ┘
│
└── tools/
    ├── serialization_free.hpp          ┐
    ├── classify.hpp                    │  Tools（5个文件）
    ├── compat_check.hpp                │  基于 Core 的便利设施
    ├── sig_export.hpp                  │
    └── sig_types.hpp                   ┘

examples/                               ┐
├── compat_auto_example.cpp             │  示例代码
└── (compat_auto.hpp + foreach.hpp)     ┘

已删除：
  detail/hash.hpp
  tools/classify_safety.hpp
```

### 依赖关系验证

```text
  signature.hpp ◄── layout_traits.hpp ◄── opaque.hpp
       ▲                  ▲
       │                  │
  tools/serialization_free.hpp
  tools/classify.hpp
  tools/compat_check.hpp ◄── tools/sig_types.hpp ◄── tools/sig_export.hpp

  方向：Tools → Core（单向）  ✓
  Tools 之间：compat_check → sig_types（线性） ✓
  循环依赖：无  ✓
```

### 文件数量对比

```text
  调整前：Core 10 + Tools 8 = 18 个文件
  调整后：Core  9 + Tools 5 = 14 个文件

  减少 4 个文件（-22%），减少的全是冗余或过早抽象。
```