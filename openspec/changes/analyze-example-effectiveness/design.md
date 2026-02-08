## Context

现有 example/ 包含 8 个示例类型。需要评估它们对 V1/V2/V3 三大核心价值的覆盖度，
并识别盲区以设计补充案例。

## Goals / Non-Goals

- Goals:
  - 量化现有案例对核心价值的覆盖率
  - 识别并填补覆盖盲区
  - 确保每个核心价值至少有一个"正面展示"和一个"边界展示"

- Non-Goals:
  - 不重构现有案例（保持向后兼容）
  - 不改变核心 API

## Decisions

### 现有案例覆盖度分析

**8 个现有类型:**
| # | 类型 | 核心特征 | 展示的价值 |
|---|------|---------|-----------|
| 1 | `PacketHeader` | 全固定宽度整数 | V1 ✅ (安全零拷贝) |
| 2 | `SharedMemRegion` | uint64/uint32 | V1 ✅ (同上) |
| 3 | `FileHeader` | char[4] + 整数 | V1 ✅ (byte array归一化) |
| 4 | `SensorRecord` | 整数 + float | V1 ✅ (IEEE 754 假设) |
| 5 | `IpcCommand` | 整数 + char[64] | V1 ✅ (大 payload) |
| 6 | `UnsafeStruct` | long + void* + wchar_t + long double | V1 ✅ (跨平台差异检测) |
| 7 | `UnsafeWithPointer` | char* 指针 | Safety ⚠️ (指针警告) |
| 8 | `MixedSafety` | int + double (非固定宽度) | V1 部分 (int 大小依赖平台) |

**覆盖矩阵:**
| 核心价值维度 | 是否被覆盖 | 覆盖方式 |
|-------------|----------|---------|
| V1: 固定宽度 POD 安全传输 | ✅ 充分 | PacketHeader, SharedMemRegion, FileHeader, SensorRecord, IpcCommand |
| V1: 跨平台差异检测 | ✅ 充分 | UnsafeStruct (long/wchar_t/long double 差异) |
| V1: 指针警告 | ✅ 基本 | UnsafeWithPointer |
| V1: 位域风险 | ❌ 缺失 | 无位域案例 |
| V1: 保守性 (int[3] vs int,int,int) | ❌ 缺失 | 无展示签名不匹配但布局相同的案例 |
| V2: 字段名区分 | ❌ 缺失 | 无展示 Layout 匹配但 Definition 不匹配的案例 |
| V2: 继承 vs 扁平 | ❌ 缺失 | 无继承相关案例 |
| V2: 枚举限定名 | ❌ 缺失 | 无枚举案例 |
| V3: 投影关系演示 | ❌ 缺失 | 无 Definition 比较演示 |
| Two-Phase: 编译时断言 | ✅ 基本 | static_assert 演示 |
| Two-Phase: 运行时报告 | ✅ 充分 | TYPELAYOUT_CHECK_COMPAT |
| Safety: 安全分级 | ✅ 基本 | ★★★/★★☆/★☆☆ 评级 |

### 覆盖度评分

**当前覆盖度: 6/12 维度 = 50%**

主要盲区:
1. **V2 完全未展示** — 现有案例只使用 Layout 层，没有展示 Definition 层的独特价值
2. **V3 完全未展示** — 没有展示"Layout 匹配但 Definition 不匹配"的场景
3. **V1 边界案例缺失** — 位域、保守性等边界场景未覆盖

### 建议补充的案例

#### 补充 1: 展示 V2 价值 — 继承展平
```cpp
// 展示 Layout 匹配但 Definition 不匹配
struct Base { int32_t id; };
struct DerivedMessage : Base { uint64_t timestamp; };
struct FlatMessage { int32_t id; uint64_t timestamp; };
// layout_match: ✅   definition_match: ❌
// 说明: 两者字节布局相同，但结构不同（继承 vs 扁平）
```

#### 补充 2: 展示 V2 价值 — 字段名区分
```cpp
// 展示相同布局不同字段名
struct PointXY { float x; float y; };
struct TempHumidity { float temperature; float humidity; };
// layout_match: ✅   definition_match: ❌
// 说明: 结构相同但语义不同
```

#### 补充 3: 展示 V1 边界 — 位域
```cpp
// 展示位域的跨平台风险
struct BitFieldExample {
    uint32_t flags : 4;
    uint32_t type  : 8;
    uint32_t value : 20;
};
// Safety: ★☆☆ (Risk) — 位域顺序是 impl-defined
```

#### 补充 4: 展示 V2 价值 — 枚举区分
```cpp
// 展示 Definition 层区分不同枚举
enum class Color : uint8_t { Red, Green, Blue };
enum class Priority : uint8_t { Low, Medium, High };
struct WithColor { Color c; uint32_t data; };
struct WithPriority { Priority p; uint32_t data; };
// layout_match: ✅   definition_match: ❌
```

### 实施建议

保留现有 8 个类型不变（向后兼容），在文件末尾添加新的分组:

```cpp
// =========================================================================
// Value Demonstration Types (展示核心价值)
// =========================================================================
```

## Risks / Trade-offs

- 增加过多示例类型可能使案例文件臃肿 → 缓解: 分组并添加注释说明每个类型的目的
- 新案例可能增加 .sig.hpp 文件大小 → 缓解: 新增类型都很小

## Open Questions

- 是否需要创建独立的 `example/value_demo.cpp` 而非在现有文件追加？
- 是否需要为 Definition 层创建专门的 Phase 2 报告？
