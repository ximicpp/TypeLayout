# 布局签名：位域（Bit-field）处理方案

## 1. 背景

布局签名的核心格式为 `@{byte_offset}:{type}`，粒度是字节级。位域成员的物理布局差异发生在位级，当前格式的分辨率不足以区分不同的位域布局。

### 问题示例

```cpp
struct A { int x : 3; int y : 5; };
struct B { int x : 4; int y : 4; };
struct C { int x : 3; int y : 6; };
```

在当前字节级格式下：

```
A: layout[s:4,a:4]{ @0:i32, @0:i32 }
B: layout[s:4,a:4]{ @0:i32, @0:i32 }  ← 与 A 相同（错误）
C: layout[s:4,a:4]{ @0:i32, @0:i32 }  ← 与 A 相同（错误）
```

三个物理布局不同的类型，签名完全相同。

---

## 2. 核心原则

| # | 原则 | 简述 |
|---|------|------|
| 1 | 反射给什么就记什么 | 不主动丢弃反射报告的信息，也不注入反射没给的信息 |
| 2 | 布局签名只编码物理布局 | 位宽和位偏移是物理布局信息，不是语义信息 |
| 3 | 标准一致性 | 标准要求位域 layout-compatible 需要相同类型且相同宽度 |

---

## 3. P2996 相关反射接口

### 已有/已提出的接口

| 接口 | 作用 | 状态 |
|------|------|------|
| `is_bit_field(r)` | 判断一个反射是否是位域成员 | 存在于提案中 |
| `offset_of(r)` | 返回成员偏移，返回类型是 `member_offset`，包含 `bytes` 和 `bits` 两个分量 | 存在，且已考虑位域 |
| `bit_size_of(r)` | 返回位域的位宽（bit width） | 存在于提案中 |
| `type_of(r)` | 返回成员的底层类型 | 通用接口，适用于位域 |

`offset_of` 返回结构化类型 `member_offset`：

```cpp
struct member_offset {
    size_t bytes;
    size_t bits;   // 字节内的位偏移 [0, CHAR_BIT)
};
```

P2996 从设计上已考虑位级粒度。

---

## 4. 双层方案

根据 P2996 位域 API 是否可用，分为完整方案和降级方案。

### 4.1 完整方案（P2996 位域 API 可用）

#### 格式扩展

```
普通成员:  @{byte_offset}:{type}
位域成员:  @{byte_offset}.{bit_offset}:{type}:{bit_width}
```

#### 编码示例

```cpp
struct A { int x : 3; int y : 5; };
```

签名：

```
layout[s:4,a:4]{ @0.0:i32:3, @0.3:i32:5 }
```

#### 生成伪代码

```cpp
for (auto mem : nonstatic_data_members_of(^T)) {
    if (is_bit_field(mem)) {
        auto off = offset_of(mem);         // member_offset{bytes, bits}
        auto width = bit_size_of(mem);     // 位宽
        auto type = type_of(mem);          // 底层类型
        emit("@{}.{}:{}:{}", off.bytes, off.bits, encode_type(type), width);
    } else {
        auto off = offset_of(mem);
        auto type = type_of(mem);
        emit("@{}:{}", off.bytes, encode_type(type));
    }
}
```

#### 区分能力验证

```cpp
struct A { int x : 3; int y : 5; };
struct B { int x : 4; int y : 4; };
struct C { int x : 3; int y : 6; };
```

签名：

```
A: layout[s:4,a:4]{ @0.0:i32:3, @0.3:i32:5 }
B: layout[s:4,a:4]{ @0.0:i32:4, @0.4:i32:4 }  ← 位偏移+位宽不同 ✅
C: layout[s:4,a:4]{ @0.0:i32:3, @0.3:i32:6 }  ← 位宽不同 ✅
```

#### 标准一致性验证

标准要求位域 layout-compatible 需要相同类型且相同宽度：

```cpp
struct X { int a : 3; int b : 5; };
struct Y { unsigned a : 3; int b : 5; };  // a 底层类型不同
```

签名：

```
X: layout[s:4,a:4]{ @0.0:i32:3, @0.3:i32:5 }
Y: layout[s:4,a:4]{ @0.0:u32:3, @0.3:i32:5 }  ← 类型不同 ✅
```

---

### 4.2 降级方案（P2996 位域 API 不可用）

#### 为什么不沿用当前格式

当前格式对位域成员产生"沉默的精度不足"——格式看起来精确（列出了每个成员的 offset 和类型），但实际上无法区分不同位宽的位域。消费者无从知晓签名在这一区域是不可靠的。

降级方案的核心改进：**诚实地标记精度边界**。

#### 对比

| 维度 | 当前格式 | 降级方案 |
|------|---------|---------|
| 精度 | 同样不足 | 同样不足 |
| 诚实性 | 沉默的精度不足，消费者无从知晓 | 显式标记，消费者知道这里是模糊区 |
| 安全性 | false positive 被当作真阳性使用 | 消费者可对 `bitfield_unit` 区域做保守处理 |
| 信息量 | 稍多（成员数、类型） | 稍少（合并为不透明块） |

丢失的那点信息量，换来的是精度边界的可见性。在不精确的领域里，知道自己不精确比假装精确更有价值。

#### 降级格式

将共享同一存储单元的位域成员合并为一个不透明块：

```
@{byte_offset}:bitfield_unit({size_in_bytes})
```

#### 降级编码示例

```cpp
struct A { int x : 3; int y : 5; };
```

降级签名：

```
layout[s:4,a:4]{ @0:bitfield_unit(4) }
```

#### 降级生成伪代码

```cpp
// 将连续位域成员合并为存储单元
size_t unit_start = 0;
size_t unit_size = 0;
bool in_bitfield = false;

for (auto mem : nonstatic_data_members_of(^T)) {
    if (is_bit_field(mem)) {
        if (!in_bitfield) {
            unit_start = offset_of(mem).bytes;
            in_bitfield = true;
        }
        // 持续追踪存储单元范围
    } else {
        if (in_bitfield) {
            // 结束位域存储单元
            unit_size = offset_of(mem).bytes - unit_start;
            emit("@{}:bitfield_unit({})", unit_start, unit_size);
            in_bitfield = false;
        }
        emit("@{}:{}", offset_of(mem).bytes, encode_type(type_of(mem)));
    }
}

// 处理尾部位域
if (in_bitfield) {
    unit_size = size_of(^T) - unit_start;  // 近似：到结构体末尾
    emit("@{}:bitfield_unit({})", unit_start, unit_size);
}
```

#### 降级方案的限制

```cpp
struct A { int x : 3; int y : 5; };
struct B { int x : 4; int y : 4; };
```

降级签名：

```
A: layout[s:4,a:4]{ @0:bitfield_unit(4) }
B: layout[s:4,a:4]{ @0:bitfield_unit(4) }  ← 无法区分（false positive）
```

这是已知的、被显式标记的精度损失。消费者看到 `bitfield_unit` 时知道需要保守处理。

---

## 5. 实现策略：运行时选择

```cpp
template <typename T>
consteval auto encode_member(meta::info mem) -> std::string {
    if (is_bit_field(mem)) {
        if constexpr (has_bit_level_reflection()) {
            // 完整方案
            auto off = offset_of(mem);
            auto width = bit_size_of(mem);
            return format("@{}.{}:{}:{}",
                off.bytes, off.bits,
                encode_type(type_of(mem)), width);
        } else {
            // 降级方案：由调用者收集后合并为 bitfield_unit
            return "__bitfield__";  // 特殊标记，由上层处理
        }
    } else {
        auto off = offset_of(mem);
        return format("@{}:{}", off.bytes, encode_type(type_of(mem)));
    }
}
```

检测 API 可用性的方式（编译期）：

```cpp
consteval bool has_bit_level_reflection() {
    // 尝试调用 bit_size_of，如果编译失败则不可用
    // 具体方式取决于 P2996 最终形态和编译器支持
    return requires { bit_size_of(nonstatic_data_members_of(^some_bitfield_struct)[0]); };
}
```

---

## 6. 总结

| 场景 | 方案 | 格式 | 精度 |
|------|------|------|------|
| P2996 位域 API 可用 | 完整方案 | `@byte.bit:type:width` | 位级，完全精确 |
| P2996 位域 API 不可用 | 降级方案 | `@byte:bitfield_unit(size)` | 存储单元级，显式标记精度损失 |
| 不推荐 | 沿用当前格式 | `@byte:type` | 字节级，沉默的精度不足 |

关键决策：**降级方案优于沿用当前格式**，因为诚实地标记精度边界比沉默地假装精确更安全、更有价值。