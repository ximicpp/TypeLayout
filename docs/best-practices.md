# Cross-Platform Zero-Copy Data Transfer — Best Practices

> 基于 Boost.TypeLayout 两阶段编译时管线的跨平台免序列化传输最佳实践指南。
>
> 本文所有结论均来自真实签名数据的实证分析，覆盖三个目标平台：
> x86_64 Linux (Clang), ARM64 macOS (Clang), x86_64 Windows (MSVC)。

## 目录

1. [核心原则](#1-核心原则)
2. [类型设计规范](#2-类型设计规范)
3. [平台差异矩阵](#3-平台差异矩阵)
4. [工程集成模式](#4-工程集成模式)
5. [CI/CD 自动化](#5-cicd-自动化)
6. [防御性编程模式](#6-防御性编程模式)
7. [常见陷阱](#7-常见陷阱)
8. [决策树](#8-决策树)

---

## 1. 核心原则

### 1.1 "签名即契约"

TypeLayout 的 Layout 签名是类型在内存中的**精确字节身份**。
两个平台上的签名相同 ⟹ 可以直接 `memcpy`/`mmap`/socket 传输原始字节。

```
签名相同 → 布局相同 → 零拷贝安全
签名不同 → 布局可能不同 → 必须序列化
```

### 1.2 两阶段管线的本质

```
Phase 1: "我的类型在这个平台上长什么样？"  → .sig.hpp (事实)
Phase 2: "它们在所有目标平台上都一样吗？"  → static_assert (证明)
```

**Phase 1 是事实收集**（需要 P2996 编译器，在每个目标平台运行）。
**Phase 2 是逻辑判定**（仅需 C++17，在任意平台运行）。

### 1.3 不要猜测，让编译器证明

❌ 错误做法：人工分析字段类型，主观判断"应该兼容"。
✅ 正确做法：在每个平台生成真实签名，用 `static_assert` 编译时证明。

```cpp
// 不要这样做：
// "PacketHeader 只有 uint32_t 和 uint16_t，应该跨平台兼容。"

// 要这样做：
static_assert(layout_match(
    linux::PacketHeader_layout,
    windows::PacketHeader_layout),
    "PacketHeader 跨平台不兼容！");
```

---

## 2. 类型设计规范

### 2.1 可移植类型清单

基于签名引擎的实际特化，以下类型在所有 64 位 little-endian 平台上产生**相同签名**：

| 类型 | 签名 | 大小 | 对齐 | 安全性 |
|------|------|------|------|--------|
| `uint8_t` / `int8_t` | `u8[s:1,a:1]` / `i8[s:1,a:1]` | 1 | 1 | ✅ 全平台安全 |
| `uint16_t` / `int16_t` | `u16[s:2,a:2]` / `i16[s:2,a:2]` | 2 | 2 | ✅ 全平台安全 |
| `uint32_t` / `int32_t` | `u32[s:4,a:4]` / `i32[s:4,a:4]` | 4 | 4 | ✅ 全平台安全 |
| `uint64_t` / `int64_t` | `u64[s:8,a:8]` / `i64[s:8,a:8]` | 8 | 8 | ✅ 全平台安全 |
| `float` | `f32[s:4,a:4]` | 4 | 4 | ✅ IEEE 754 |
| `double` | `f64[s:8,a:8]` | 8 | 8 | ✅ IEEE 754 |
| `char` | `char[s:1,a:1]` | 1 | 1 | ✅ 全平台安全 |
| `char8_t` | `char8[s:1,a:1]` | 1 | 1 | ✅ 全平台安全 |
| `char16_t` | `char16[s:2,a:2]` | 2 | 2 | ✅ 全平台安全 |
| `char32_t` | `char32[s:4,a:4]` | 4 | 4 | ✅ 全平台安全 |
| `bool` | `bool[s:1,a:1]` | 1 | 1 | ✅ 全平台安全 |
| `std::byte` | `byte[s:1,a:1]` | 1 | 1 | ✅ 全平台安全 |
| `char[N]` / `uint8_t[N]` | `bytes[s:N,a:1]` | N | 1 | ✅ 字节数组归一化 |
| `T[N]`（T 为安全类型） | `array[s:S,a:A]<T_sig,N>` | S | A | ✅ 元素安全则数组安全 |

### 2.2 危险类型清单

以下类型在不同平台产生**不同签名**，绝不应出现在跨平台传输结构中：

| 类型 | 差异来源 | Linux x86_64 | macOS ARM64 | Windows x86_64 |
|------|---------|:---:|:---:|:---:|
| `long` | LP64 vs LLP64 | 8B | 8B | **4B** |
| `unsigned long` | LP64 vs LLP64 | 8B | 8B | **4B** |
| `wchar_t` | OS ABI | 4B | 4B | **2B** |
| `long double` | 架构 ABI | **16B** | **8B** | **8B** |
| `T*`（任何指针） | 语义无意义 | 8B | 8B | 8B |
| `size_t` | 底层为 `unsigned long` | 8B | 8B | 8B* |
| `ptrdiff_t` | 底层为 `long` | 8B | 8B | 8B* |

> *`size_t` 和 `ptrdiff_t` 在 64 位平台上大小相同（8B），但底层类型不同导致签名可能不同。

### 2.3 模式：可移植协议类型

```cpp
// ✅ 完美可移植——只使用定宽类型
struct PacketHeader {
    uint32_t magic;       // 协议魔数
    uint16_t version;     // 版本号
    uint16_t type;        // 消息类型
    uint32_t payload_len; // 载荷长度
    uint32_t checksum;    // CRC32
};
// 签名: record[s:16,a:4]{@0:u32,@4:u16,@6:u16,@8:u32,@12:u32}
// 三平台完全相同 ✅
```

```cpp
// ✅ 完美可移植——定宽类型 + char 数组 + IEEE 浮点
struct SensorRecord {
    uint64_t timestamp_ns;
    float    temperature;
    float    humidity;
    float    pressure;
    uint32_t sensor_id;
};
// 签名: record[s:24,a:8]{@0:u64,@8:f32,@12:f32,@16:f32,@20:u32}
// 三平台完全相同 ✅
```

```cpp
// ❌ 不可移植——包含平台相关类型
struct UnsafeStruct {
    long        a;    // ❌ LP64: 8B, LLP64: 4B
    void*       ptr;  // ❌ 跨进程无意义
    wchar_t     wc;   // ❌ Linux: 4B, Windows: 2B
    long double ld;   // ❌ x86_64: 16B, ARM64: 8B
};
// Linux:   record[s:48,a:16]{...}
// macOS:   record[s:32,a:8]{...}
// Windows: record[s:32,a:8]{...}  (但字段签名与 macOS 也不同)
```

### 2.4 黄金规则

> **如果一个类型只使用 §2.1 中的安全类型作为字段，
> 且不包含位域（bit-field），则它在所有同字节序的 64 位平台上布局相同。**

---

## 3. 平台差异矩阵

### 3.1 数据模型对比

```
              LP64 (Linux, macOS)        LLP64 (Windows)
              ──────────────────         ─────────────────
short              2 bytes                    2 bytes
int                4 bytes                    4 bytes
long             ► 8 bytes ◄               ► 4 bytes ◄     ← 关键差异
long long          8 bytes                    8 bytes
pointer            8 bytes                    8 bytes
```

### 3.2 特殊类型对比

```
                 x86_64 Linux    ARM64 macOS     x86_64 Windows
                 ────────────    ───────────     ──────────────
wchar_t          4 bytes         4 bytes        ► 2 bytes ◄
long double    ► 16 bytes ◄    ► 8 bytes ◄     ► 8 bytes ◄
max_align_t      16 bytes        16 bytes         16 bytes
```

### 3.3 签名差异实证

UnsafeStruct 在三个平台上的 Layout 签名：

```
Linux x86_64:
  record[s:48,a:16]{@0:i64[s:8,a:8],@8:ptr[s:8,a:8],@16:wchar[s:4,a:4],@32:f80[s:16,a:16]}
  ├─ long=i64(8B), padding 12B for 16-align, long double=f80(16B)

macOS ARM64:
  record[s:32,a:8]{@0:i64[s:8,a:8],@8:ptr[s:8,a:8],@16:wchar[s:4,a:4],@24:f80[s:8,a:8]}
  ├─ long=i64(8B), padding 4B for 8-align, long double=f80(8B)

Windows x86_64:
  record[s:32,a:8]{@0:i32[s:4,a:4],@8:ptr[s:8,a:8],@16:wchar[s:2,a:2],@24:f80[s:8,a:8]}
  ├─ long=i32(4B)❗, wchar=2B❗, long double=f80(8B)
```

注意：macOS 和 Windows 大小相同(32B)但**内部布局不同**（`long` 和 `wchar_t` 签名不同），
所以 memcpy 仍不安全——这正是签名比对优于 `sizeof` 比对的原因。

---

## 4. 工程集成模式

### 4.1 推荐项目结构

```
my_project/
├── include/
│   └── protocol/
│       └── types.hpp          # 共享的跨平台类型定义
├── sigs/                       # 各平台导出的签名（提交到 Git）
│   ├── x86_64_linux_clang.sig.hpp
│   ├── arm64_macos_clang.sig.hpp
│   └── x86_64_windows_msvc.sig.hpp
├── tools/
│   ├── sig_export.cpp         # Phase 1: 签名导出程序
│   └── compat_check.cpp       # Phase 2: 兼容性检查程序
└── CMakeLists.txt
```

### 4.2 共享类型定义

将所有跨平台传输类型放在一个专用头文件中：

```cpp
// protocol/types.hpp — 跨平台传输类型（只使用安全类型）
#pragma once
#include <cstdint>

namespace protocol {

struct PacketHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t type;
    uint32_t payload_len;
    uint32_t checksum;
};

struct SensorRecord {
    uint64_t timestamp_ns;
    float    temperature;
    float    humidity;
    float    pressure;
    uint32_t sensor_id;
};

// 如果需要字符串，使用 char[N] 而非 std::string
struct DeviceInfo {
    char     name[32];      // bytes[s:32,a:1] — 全平台安全
    uint32_t firmware_ver;
    uint64_t serial_no;
};

} // namespace protocol
```

### 4.3 签名导出程序

```cpp
// tools/sig_export.cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "protocol/types.hpp"

int main(int argc, char* argv[]) {
    boost::typelayout::SigExporter ex;
    ex.add<protocol::PacketHeader>("PacketHeader");
    ex.add<protocol::SensorRecord>("SensorRecord");
    ex.add<protocol::DeviceInfo>("DeviceInfo");

    if (argc >= 2) {
        std::filesystem::create_directories(argv[1]);
        std::string path = std::string(argv[1]) + "/" + ex.platform_name() + ".sig.hpp";
        return ex.write(path);
    }
    ex.write_stdout();
    return 0;
}
```

### 4.4 兼容性守卫（推荐模式）

在共享类型定义文件中内嵌编译时守卫：

```cpp
// protocol/compat_guard.hpp — 在包含签名头文件后使用
#pragma once
#include <boost/typelayout/tools/compat_check.hpp>

// 使用宏简化多平台断言
#define ASSERT_PORTABLE(Type)                                         \
    static_assert(boost::typelayout::compat::layout_match(            \
        PLATFORM_A::Type##_layout, PLATFORM_B::Type##_layout),        \
        #Type " is not binary-compatible across target platforms!")

// 在 compat_check.cpp 中：
// namespace PLATFORM_A = boost::typelayout::platform::x86_64_linux_clang;
// namespace PLATFORM_B = boost::typelayout::platform::arm64_macos_clang;
// ASSERT_PORTABLE(PacketHeader);
// ASSERT_PORTABLE(SensorRecord);
```

---

## 5. CI/CD 自动化

### 5.1 GitHub Actions 多平台管线

```yaml
name: Cross-Platform Compatibility Check

on: [push, pull_request]

jobs:
  # Phase 1: 在各平台上导出签名
  export-linux:
    runs-on: ubuntu-latest
    container: ghcr.io/ximicpp/typelayout-p2996:latest
    steps:
      - uses: actions/checkout@v4
      - run: |
          clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
            -I./include -o sig_export tools/sig_export.cpp
          mkdir -p sigs
          ./sig_export sigs/
      - uses: actions/upload-artifact@v4
        with:
          name: sig-linux
          path: sigs/*.sig.hpp

  export-macos:
    runs-on: macos-latest  # Apple Silicon
    steps:
      - uses: actions/checkout@v4
      - run: |
          # 假设 macOS runner 已安装 P2996 编译器
          clang++ -std=c++26 -freflection -freflection-latest \
            -I./include -o sig_export tools/sig_export.cpp
          mkdir -p sigs
          ./sig_export sigs/

      - uses: actions/upload-artifact@v4
        with:
          name: sig-macos
          path: sigs/*.sig.hpp

  # Phase 2: 下载所有签名，编译时检查
  check-compat:
    needs: [export-linux, export-macos]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/download-artifact@v4
        with:
          path: sigs/
          merge-multiple: true

      - run: |
          # Phase 2 只需要 C++17，普通 g++ 就行
          g++ -std=c++17 -I./include -I. \
            -o compat_check tools/compat_check.cpp
          ./compat_check
```

### 5.2 签名版本控制策略

**推荐：将 `.sig.hpp` 文件提交到 Git**

```
✅ 提交签名文件的优势：
  - 每次 PR 可以 diff 签名变化
  - Code review 时能看到布局变更
  - 历史可追溯
  - Phase 2 不需要重新运行 Phase 1

⚠️ 注意事项：
  - 只在签名实际变化时更新（由 CI 自动化）
  - .sig.hpp 中的时间戳会导致无意义的 diff — 可考虑去掉
  - 添加 .gitattributes 标记为生成文件
```

---

## 6. 防御性编程模式

### 6.1 类型版本化

为跨平台类型添加版本字段，防止新旧版本混淆：

```cpp
struct PacketHeader {
    uint32_t magic    = 0x544C4159;  // "TLAY"
    uint16_t version  = 2;           // 协议版本
    uint16_t type;
    uint32_t payload_len;
    uint32_t checksum;
};
```

### 6.2 对齐感知设计

显式控制字段顺序，避免隐式 padding 导致的浪费和意外：

```cpp
// ❌ 低效布局 — 有 padding 间隙
struct Bad {
    uint8_t  flags;       // 1B → 7B padding
    uint64_t timestamp;   // 8B
    uint16_t id;          // 2B → 6B padding
    uint64_t value;       // 8B
};                        // Total: 32B (12B wasted on padding)

// ✅ 紧凑布局 — 无 padding
struct Good {
    uint64_t timestamp;   // 8B, offset 0
    uint64_t value;       // 8B, offset 8
    uint16_t id;          // 2B, offset 16
    uint8_t  flags;       // 1B, offset 18
    uint8_t  reserved;    // 1B, offset 19 (显式 padding)
};                        // Total: 20B (0B wasted)
```

### 6.3 显式 padding

在需要对齐的地方使用显式 reserved 字段而非依赖编译器隐式 padding：

```cpp
struct ExplicitPadding {
    uint32_t id;
    uint32_t _reserved0;   // 显式 4B padding
    uint64_t timestamp;    // 8B aligned
};
// 签名清晰：{@0:u32,@4:u32,@8:u64}
```

### 6.4 避免位域

位域布局是实现定义的（C++ 标准 §12.2.4），不同编译器的打包策略可能不同：

```cpp
// ❌ 位域不可移植
struct BitFlags {
    uint32_t active : 1;
    uint32_t mode   : 3;
    uint32_t level  : 4;
};

// ✅ 用位操作替代
struct Flags {
    uint32_t flags;  // 手动位操作
    // active = flags & 0x01
    // mode   = (flags >> 1) & 0x07
    // level  = (flags >> 4) & 0x0F
};
```

---

## 7. 常见陷阱

### 7.1 `int` 的陷阱

`int` 在所有主流 64 位平台上都是 4B，所以**实际上是安全的**——但不保证将来不变。
标准只保证 `int` ≥ 16 位。**最佳实践是永远使用 `int32_t`**。

```cpp
// ⚠️ 实际安全但不推荐
struct MixedSafety {
    uint32_t id;
    double   value;
    int      count;    // 实际 4B，但标准不保证
};

// ✅ 推荐
struct MixedSafety {
    uint32_t id;
    double   value;
    int32_t  count;    // 明确 4B
};
```

### 7.2 指针的陷阱

指针在所有 64 位平台上大小相同(8B)，签名也匹配——**但值在跨进程/跨机器传输时无意义**：

```cpp
// ⚠️ 签名匹配但语义错误
struct UnsafeWithPointer {
    uint32_t id;
    char*    name;       // 指针值跨进程无意义！
    uint64_t timestamp;
};
// Linux 和 Windows 的签名完全相同 — TypeLayout 无法检测这种语义错误

// ✅ 正确做法：用 char 数组或偏移量替代指针
struct SafeWithName {
    uint32_t id;
    char     name[64];   // 内联字符串
    uint64_t timestamp;
};
```

### 7.3 enum 的陷阱

无作用域枚举（`enum`）的底层类型由编译器决定；必须用 `enum class` 并显式指定底层类型：

```cpp
// ❌ 底层类型不确定
enum Status { OK, ERR, TIMEOUT };

// ✅ 明确底层类型
enum class Status : uint8_t { OK, ERR, TIMEOUT };
```

### 7.4 嵌套 struct 的注意事项

嵌套 struct 在 Layout 层会被**展平**，所以只要所有叶字段都是安全类型就可以：

```cpp
struct Inner {
    uint32_t a;
    uint32_t b;
};
struct Outer {
    Inner x;        // Layout 层展平为 @0:u32, @4:u32
    uint64_t y;
};
// Layout 签名与 struct Flat { uint32_t a, b; uint64_t y; } 完全相同
// 只要 Inner 的所有字段都是安全类型，Outer 就是安全的
```

---

## 8. 决策树

```
需要跨平台传输类型 T？
│
├─ T 只包含 §2.1 中的安全类型？
│   ├─ 是 → 高概率可移植，但仍需 Phase 1+2 验证
│   │       运行 sig_export → static_assert → 确认 ✅
│   └─ 否 → 包含哪些危险类型？
│       ├─ long / unsigned long → 替换为 int64_t / uint64_t
│       ├─ wchar_t → 替换为 char16_t 或 char32_t
│       ├─ long double → 替换为 double
│       ├─ 指针 → 替换为 char[N]、偏移量或 ID
│       ├─ int → 替换为 int32_t（推荐但非必须）
│       ├─ 位域 → 替换为整数 + 位操作
│       └─ 修改后回到第一步
│
├─ T 包含继承？
│   └─ Layout 层会展平继承——如果基类字段都是安全类型，仍然可移植
│
├─ T 包含 union？
│   └─ union 成员不展平——需要确保每个成员本身都是可移植的
│
├─ T 跨 32 位和 64 位平台？
│   └─ 架构前缀不同（[32-le] vs [64-le]），签名必然不同
│       需要使用序列化方案
│
└─ T 跨不同字节序？（little-endian vs big-endian）
    └─ 架构前缀不同（[64-le] vs [64-be]），需要字节序转换
```

---

## 总结

| 原则 | 规则 |
|------|------|
| **类型选择** | 只使用 `<cstdint>` 定宽类型 + `float`/`double` + `char[N]` |
| **避免** | `long`、`wchar_t`、`long double`、指针、位域、`int` |
| **验证方式** | Phase 1 导出签名 + Phase 2 `static_assert` 编译时证明 |
| **集成位置** | CI/CD 自动化 Phase 1+2，PR diff 可见签名变化 |
| **版本管理** | 将 `.sig.hpp` 提交到 Git，跟踪布局演化 |
| **防御编程** | 显式 padding、类型版本字段、枚举指定底层类型 |
