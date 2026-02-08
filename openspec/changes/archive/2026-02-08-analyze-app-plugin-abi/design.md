## Context

C++ 插件系统 (dlopen/LoadLibrary) 在宿主和插件之间传递数据结构。如果双方对结构体的
内存布局理解不一致 (如不同编译器版本、不同编译选项导致的 ABI 差异)，会导致未定义行为。
TypeLayout 可以作为"ABI 守门人"，在加载时验证结构兼容性。

## Goals / Non-Goals

- Goals:
  - 论证 TypeLayout 在插件 ABI 验证场景的价值
  - 分析 ODR 违规检测能力
  - 展示"加载时签名校验"的使用模式

- Non-Goals:
  - 不替代 ABI 检查工具 (如 abi-compliance-checker)
  - 不验证虚函数表兼容性
  - 不处理 C++ 名称修饰差异

## Decisions

### 场景描述

**典型插件系统架构:**
```
宿主 (Host)                              插件 (Plugin.so / Plugin.dll)
┌──────────────────────┐                ┌──────────────────────┐
│ struct PluginConfig { │                │ struct PluginConfig { │
│   uint32_t version;   │   dlsym()     │   uint32_t version;   │
│   uint32_t flags;     │ ◄──────────► │   uint32_t flags;     │
│   double   param_a;   │                │   double   param_a;   │
│   double   param_b;   │                │   double   param_b;   │
│ };                    │                │ };                    │
│                       │                │                       │
│ // 编译器 A           │                │ // 编译器 B           │
│ // -O2 -std=c++17     │                │ // -O3 -std=c++20     │
└──────────────────────┘                └──────────────────────┘
```

**痛点:**
1. **ABI 断裂**: 升级编译器可能改变 padding 策略
2. **编译选项差异**: `-fpack-struct` 等选项影响布局
3. **ODR 违规**: 宿主和插件包含同名结构的不同定义
4. **版本不匹配**: 宿主更新了接口结构但未通知插件

### TypeLayout 的插件验证工作流

#### 模式 1: 编译时验证 (宿主和插件同时编译)

```cpp
// plugin_interface.hpp — 共享头文件
struct PluginConfig {
    uint32_t version;
    uint32_t flags;
    double   param_a;
    double   param_b;
};

// 宿主端: 导出签名
TYPELAYOUT_EXPORT_TYPES(PluginConfig)

// 插件端: 编译时断言
#include "host_sigs/x86_64_linux_clang.sig.hpp"
TYPELAYOUT_ASSERT_COMPAT(x86_64_linux_clang, current_platform)
```

#### 模式 2: 运行时签名校验 (插件动态加载)

```cpp
// 宿主定义插件入口
struct PluginEntry {
    const char* layout_sig;  // 插件报告的签名字符串
    void* (*create)();
    void  (*destroy)(void*);
};

// 宿主加载时校验
void load_plugin(const char* path) {
    auto* entry = (PluginEntry*)dlsym(handle, "plugin_entry");
    
    // 比较签名
    constexpr auto host_sig = get_layout_signature<PluginConfig>();
    if (host_sig.to_string_view() != entry->layout_sig) {
        throw std::runtime_error("Plugin ABI mismatch!");
    }
    // 签名匹配 → 安全使用
}
```

### ODR 违规检测

**这是 Definition 签名的独特价值:**

```cpp
// host.cpp
struct SharedData { int32_t x; double y; };  // version A

// plugin.cpp  
struct SharedData { int32_t x; float y; };   // version B (bug: double→float)

// 如果只用名字比较: "SharedData" == "SharedData" → 误判为相同 ❌
// TypeLayout Layout 签名: 不同 (double vs float) → 正确检测 ✅
// TypeLayout Definition 签名: 更进一步，可区分字段名变更
```

ODR 违规是 C++ 中最隐蔽的 bug 之一——编译器和链接器通常不检查。TypeLayout 的
Definition 签名可以在编译时或加载时检测这类问题。

### 与传统方案对比

| 维度 | 版本号检查 | COM/C ABI | abi-compliance-checker | TypeLayout |
|------|----------|-----------|----------------------|------------|
| 检测精度 | 版本粒度 | 接口级 | 完整 ABI | **字段级** |
| 检测时机 | 运行时 | 编译时 (IDL) | 离线分析 | **编译时/运行时** |
| C++ struct 支持 | ❌ | 有限 | ✅ | **✅** |
| ODR 检测 | ❌ | ❌ | ✅ | **✅** |
| 集成成本 | 低 | 高 (IDL) | 中 (需 DWARF) | **低** |

### TypeLayout 的局限

| 维度 | 能力 | 说明 |
|------|------|------|
| 数据结构布局 | ✅ 完全 | 检测所有字段偏移/大小差异 |
| 虚函数表布局 | ⚠️ 标记但不验证 | 检测 vptr 存在但不验证 vtable 内容 |
| C++ 名称修饰 | ❌ | 不检测函数签名的 ABI 差异 |
| 异常处理 ABI | ❌ | 不检测异常机制兼容性 |
| STL 类型布局 | ⚠️ 取决于实现 | STL 类型是 impl-defined，跨编译器不保证 |

**定位:** TypeLayout 不是完整的 ABI 检查工具，而是专注于**数据结构布局**这一关键维度。
对于大多数插件系统，数据结构兼容性是最常见的 ABI 问题来源。

### 适用性矩阵

| 插件类型 | TypeLayout 适用性 | 原因 |
|---------|------------------|------|
| 数据处理插件 (传入/传出 POD) | ✅ 非常适合 | 纯数据交互 |
| GUI 插件 (共享控件类型) | ⚠️ 部分适合 | 数据部分可验证，虚函数不行 |
| 脚本引擎插件 | ⚠️ 仅接口层 | 脚本对象通常有自己的类型系统 |
| 驱动程序 (内核模块) | ✅ 适合 | 内核 ABI 依赖固定结构 |

## Risks / Trade-offs

- 不验证虚函数表 → 对含虚函数的接口需要额外手段
- 不处理名称修饰 → 不替代完整 ABI 检查工具
- 运行时签名比较需要额外的签名传递机制 → 增加少量接口复杂度

## Open Questions

- 是否提供 `TYPELAYOUT_PLUGIN_ENTRY` 宏简化运行时签名传递？→ 低优先级
- 是否支持 STL 类型 (vector, string) 的插件接口？→ 不建议 (impl-defined)
