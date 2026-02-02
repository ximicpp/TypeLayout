# Project Context

## Purpose
Boost.TypeLayout 是一个 C++26 header-only 库，使用 P2996 静态反射提供编译时内存布局分析和验证。它生成人类可读的布局签名 (Layout Signature) 来唯一标识类型的内存布局，支持健壮的二进制接口验证和 ABI 兼容性检查。

**核心保证**: `Identical signature ⟺ Identical memory layout`（相同签名等价于相同内存布局）

## Tech Stack
- **语言**: C++26
- **反射**: P2996 静态反射 (`<experimental/meta>`)
- **编译器**: Bloomberg Clang P2996 fork（目前唯一支持 P2996 的编译器）
- **构建系统**: CMake, B2 (Boost.Build)
- **库类型**: Header-only
- **编译选项**: `-std=c++26 -freflection -freflection-latest -stdlib=libc++`

## Project Structure
```
TypeLayout/
├── include/boost/
│   ├── typelayout.hpp                 # 便捷头文件（转发到 typelayout/ 内部）
│   └── typelayout/
│       ├── typelayout.hpp             # Core 层入口
│       ├── typelayout_util.hpp        # Utility 层入口（含 Core）
│       ├── typelayout_all.hpp         # 完整功能入口
│       ├── core/                      # Layer 1: 布局签名核心
│       │   ├── config.hpp             # 编译器检测
│       │   ├── compile_string.hpp     # CompileString<N>, fixed_string<N>
│       │   ├── hash.hpp               # FNV-1a, DJB2 哈希
│       │   ├── reflection_helpers.hpp # P2996 反射辅助
│       │   ├── type_signature.hpp     # TypeSignature<T> 特化
│       │   ├── signature.hpp          # get_layout_signature<T>()
│       │   ├── verification.hpp       # LayoutVerification
│       │   └── concepts.hpp           # LayoutCompatible, LayoutMatch
│       ├── util/                      # Layer 2: 序列化实用工具
│       │   ├── platform_set.hpp       # PlatformSet, SerializationBlocker
│       │   ├── serialization_check.hpp# is_serializable<T, P>
│       │   └── concepts.hpp           # Serializable, ZeroCopyTransmittable
│       └── detail/                    # 已废弃的兼容头文件
├── test/
│   └── test_all_types.cpp             # 全面的编译时测试
├── example/
│   ├── demo.cpp                       # 完整功能示例
│   ├── core_demo.cpp                  # 纯核心层示例
│   └── util_demo.cpp                  # 序列化工具示例
├── doc/
│   ├── api_reference.md
│   ├── quickstart.md
│   └── technical_overview.md          # 技术演讲大纲
├── meta/
│   └── libraries.json                 # Boost 元数据
├── build.jam                          # B2 构建文件
├── CMakeLists.txt                     # CMake 构建文件
└── README.md
```

### 分层架构说明

**核心价值定位**: Layout Signature（布局签名）是核心产品，Serialization Safety（序列化安全）是基于核心的实用工具。

- **Core Layer (`core/`)**: 纯粹的内存布局分析引擎，无序列化策略依赖
- **Utility Layer (`util/`)**: 基于 Core 构建的序列化安全检查功能
- **detail/**: 保留用于内部实现，旧头文件提供向后兼容重定向

## Project Conventions

### Code Style
- 遵循 Boost 库代码风格
- 使用 `boost::typelayout` 命名空间
- 所有核心功能使用 `consteval` 实现编译时计算
- 优先使用 C++20 Concepts 进行类型约束
- 使用 `[[nodiscard]]` 标记返回值重要的函数

### Architecture Patterns
- **Header-only 设计**: 所有实现在头文件中
- **编译时计算优先**: 零运行时开销
- **静态反射**: 使用 P2996 API 进行类型分析
- **递归类型签名**: 支持嵌套结构体和继承层次
- **双哈希验证**: FNV-1a + DJB2 提供 ~2^128 抗碰撞性

### Testing Strategy
- **编译时测试**: 使用 `static_assert` 进行编译时测试（编译成功=测试通过）
- **类型覆盖**: 测试覆盖所有基本类型、复合类型、继承、位域等
- **跨平台验证**: 架构前缀 `[64-le]`/`[32-be]` 区分平台

### Git Workflow
- main 分支为稳定分支
- feature/* 分支用于新功能开发
- 提交信息使用描述性语言

## Core Components

### 1. 布局签名格式 (Layout Signature Format)
签名格式设计为人类可读、机器可比较：
```
[ARCH]type[s:SIZE,a:ALIGN]{@OFFSET[name]:type_sig,...}
```

**示例**:
```cpp
struct Point { int32_t x, y; };
// 签名: "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}"
```

**组成部分**:
- `[64-le]` - 架构前缀（64位小端序）
- `struct[s:8,a:4]` - 类型类别、大小8字节、对齐4
- `@0[x]` - 偏移0、字段名"x"
- `:i32[s:4,a:4]` - 类型签名（含大小/对齐）

### 2. 支持的类型签名
| 类型 | 签名格式 | 示例 |
|------|----------|------|
| 定宽整数 | `i8/u8/i16/u16/i32/u32/i64/u64` | `i32[s:4,a:4]` |
| 浮点数 | `f32/f64/f80` | `f64[s:8,a:8]` |
| 字符 | `char/char8/char16/char32/wchar` | `char[s:1,a:1]` |
| 布尔 | `bool` | `bool[s:1,a:1]` |
| 指针 | `ptr/fnptr/ref/rref/memptr` | `ptr[s:8,a:8]` |
| 字符数组 | `bytes` | `bytes[s:16,a:1]` |
| 普通数组 | `array<element,N>` | `array[s:16,a:4]<i32[s:4,a:4],4>` |
| 枚举 | `enum<underlying>` | `enum[s:1,a:1]<u8[s:1,a:1]>` |
| 联合体 | `union` | `union[s:4,a:4]` |
| 结构体 | `struct{...}` | `struct[s:8,a:4]{...}` |
| 继承类 | `class[...,inherited]{...}` | 包含 `@0[base]:...` |
| 多态类 | `class[...,polymorphic]{...}` | 包含虚表指针 |
| 位域 | `bits<width,type>` | `@4.2[flags]:bits<3,u8[s:1,a:1]>` |
| 智能指针 | `unique_ptr/shared_ptr/weak_ptr` | `shared_ptr[s:16,a:8]` |

### 3. 分层 API 架构

Boost.TypeLayout 采用分层架构设计：

**Layer 1: Layout Signature（布局签名层）**
- 完整的类型内存布局描述（bit-level）
- 用于 ABI 兼容性和二进制协议验证

**Layer 2: Serialization Status（序列化状态层）**
- 类型是否可安全序列化的审计结果
- 检测指针、引用、位域、平台相关类型等

### 4. 核心 API
| 函数 | 说明 |
|------|------|
| `get_layout_signature<T>()` | 获取带架构前缀的编译时布局签名 |
| `get_layout_hash<T>()` | 获取64位 FNV-1a 哈希 |
| `get_layout_verification<T>()` | 获取双哈希验证（FNV-1a + DJB2 + 长度） |
| `signatures_match<T1, T2>()` | 检查两个类型是否有相同布局签名 |
| `is_serializable_v<T, P>` | 检查类型是否可在指定平台集上序列化 |
| `has_bitfields<T>()` | 检查类型是否包含位域 |
| `serialization_status<T, P>()` | 获取序列化状态指示符（如 `[64-le]serial` 或 `!serial:ptr`） |

### 5. C++20 Concepts
| Concept | 说明 |
|---------|------|
| `Serializable<T>` | 类型可安全序列化（无指针、引用、位域、平台相关类型） |
| `ZeroCopyTransmittable<T>` | 类型可零拷贝传输（Serializable + trivially copyable + standard layout） |
| `LayoutCompatible<T, U>` | 两个类型有相同内存布局 |
| `LayoutMatch<T, Sig>` | 类型布局匹配预期签名字符串 |
| `LayoutHashMatch<T, Hash>` | 类型布局哈希匹配预期值 |

### 6. 关键宏
```cpp
TYPELAYOUT_BIND(Type, ExpectedSig)  // 静态断言布局匹配
```

## Domain Context

### 核心概念
- **Layout Signature (布局签名)**: 类型内存布局的完整字符串表示
- **Portability (可移植性)**: 类型是否跨平台兼容（无平台相关成员）
- **ABI Compatibility (ABI兼容性)**: 二进制级别的接口兼容
- **Static Reflection (静态反射)**: C++26 编译时反射提案 P2996

### 平台相关类型
以下类型在不同平台有不同大小，被标记为非可移植：
- `wchar_t`: Windows 2字节 vs Linux 4字节
- `long` / `unsigned long`: Windows LLP64 4字节 vs Linux LP64 8字节
- `long double`: 8/12/16字节取决于平台

### 位域处理
位域被标记为非可移植，因为：
- 位域布局是实现定义的（C++11 §9.6）
- 不同编译器可能有不同的打包策略
- 签名格式: `@byte.bit[name]:bits<width,type>`

### P2996 反射 API
本库使用的关键反射 API：
- `std::meta::nonstatic_data_members_of(^^T)` - 枚举所有成员
- `std::meta::identifier_of(member)` - 获取成员名称
- `std::meta::offset_of(member).bytes` - 获取编译器验证的偏移
- `std::meta::type_of(member)` - 获取成员类型
- `std::meta::bases_of(^^T)` - 获取基类列表
- `std::meta::is_bit_field(member)` - 检测位域
- `std::meta::bit_size_of(member)` - 获取位域宽度
- `[:type:]` - 拼接语法用于实例化类型

## Important Constraints
- 需要支持 P2996 的编译器（目前仅 Bloomberg Clang fork）
- 必须是 header-only 以符合 Boost 库要求
- 所有分析必须在编译时完成（零运行时开销）
- 需要支持 IEEE 754 浮点数
- 指针大小编码在签名中（4或8字节）

## Use Cases

### 1. 二进制协议验证
```cpp
struct NetworkHeader { uint32_t magic; uint64_t timestamp; };
TYPELAYOUT_BIND(NetworkHeader, "[64-le]struct[s:16,a:8]{...}");
```

### 2. 跨平台序列化
```cpp
template<Serializable T>
void safe_binary_write(std::ostream& os, const T& obj) {
    os.write(reinterpret_cast<const char*>(&obj), sizeof(T));
}
```

### 3. 共享内存验证
```cpp
template<LayoutHashMatch<T, EXPECTED_HASH> T>
T* map_shared_memory(const char* name) { ... }
```

### 4. 模板约束
```cpp
template<typename T>
    requires LayoutMatch<T, "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}">
void process_point(const T& p) { ... }
```

## External Dependencies
- **Bloomberg Clang P2996**: 唯一支持 P2996 的编译器实现
- **Boost License**: 使用 Boost Software License 1.0
- **可选**: Boost.Interprocess（提供 `offset_ptr` 特化）

## 构建与测试指南

### 编译器要求

本项目**必须**使用支持 P2996 (静态反射) 的 Clang 编译器，目前仅 Bloomberg Clang fork 支持：
- **Bloomberg Clang P2996**: https://github.com/bloomberg/clang-p2996
- **编译选项**: `-std=c++26 -freflection -freflection-latest -stdlib=libc++`

### 方式 1: 本地 WSL 构建 (推荐开发环境)

如果在 Windows 上，可以使用 WSL (Windows Subsystem for Linux) 运行 Docker：

```bash
# 在 WSL 中启动 Docker
wsl

# 进入项目目录（Windows 路径映射）
cd /mnt/g/workspace/TypeLayout

# 拉取预构建的 P2996 镜像
docker pull ghcr.io/ximicpp/typelayout-p2996:latest

# 构建和测试（需要设置 LD_LIBRARY_PATH 以找到 libc++）
docker run --rm \
    -v $(pwd):/src:ro \
    -w /tmp/build \
    -e LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu \
    ghcr.io/ximicpp/typelayout-p2996:latest \
    bash -c "cp -r /src/* . && cmake -B build -G Ninja && cmake --build build && cd build && ctest --output-on-failure"

# 交互式开发
docker run -it --rm \
    -v $(pwd):/workspace -w /workspace \
    -e LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu \
    ghcr.io/ximicpp/typelayout-p2996:latest
```

**重要说明**:
- **LD_LIBRARY_PATH**: P2996 工具链的 `libc++.so.1` 位于非标准路径，必须设置此环境变量
- **只读挂载 + 拷贝**: 使用 `-v $(pwd):/src:ro` 只读挂载，然后拷贝到 `/tmp/build`，避免 Windows/Linux 路径混淆导致的 CMake 缓存问题

### 方式 2: 本地 Docker 构建 (Windows/Linux/macOS)

如果已安装 Docker Desktop：

```bash
# 拉取镜像
docker pull ghcr.io/ximicpp/typelayout-p2996:latest

# 运行构建和测试
docker run --rm -v ${PWD}:/workspace -w /workspace \
    ghcr.io/ximicpp/typelayout-p2996:latest \
    bash -c "cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure"
```

Windows PowerShell 版本：
```powershell
docker run --rm -v "${PWD}:/workspace" -w /workspace `
    ghcr.io/ximicpp/typelayout-p2996:latest `
    bash -c "cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure"
```

### 方式 3: GitHub Actions CI (远端构建)

每次推送到 `main`/`develop`/`feature/*` 分支会自动触发 CI：

1. CI 工作流文件: `.github/workflows/ci.yml`
2. 使用镜像: `ghcr.io/ximicpp/typelayout-p2996:latest`
3. 构建步骤:
   - `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`
   - `cmake --build build`
   - `ctest --test-dir build --output-on-failure`

查看 CI 状态: https://github.com/ximicpp/TypeLayout/actions

### Docker 镜像内容

`ghcr.io/ximicpp/typelayout-p2996:latest` 包含：
- Bloomberg Clang P2996 fork（带 `-freflection` 支持）
- CMake 3.x
- Ninja 构建系统
- libc++ 标准库

### CMake 构建选项

```bash
# 完整构建命令
cmake -B build \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++" \
    -DBUILD_TESTING=ON

cmake --build build --parallel

# 运行测试
ctest --test-dir build --output-on-failure
```

### 测试目标

| 目标 | 说明 | 类型 |
|------|------|------|
| `test_all_types` | 全类型布局签名测试 | 编译时 (static_assert) |
| `demo` | 完整功能演示 | 运行时 |
| `core_demo` | 核心层演示 | 运行时 |
| `util_demo` | 序列化工具演示 | 运行时 |

### 快速验证命令

```bash
# 1. 拉取镜像并构建测试 (一行命令)
docker run --rm -v $(pwd):/workspace -w /workspace \
    ghcr.io/ximicpp/typelayout-p2996:latest \
    bash -c "cmake -B build -G Ninja && cmake --build build && ctest --test-dir build -V"

# 2. 仅编译检查 (验证 static_assert 测试)
docker run --rm -v $(pwd):/workspace -w /workspace \
    ghcr.io/ximicpp/typelayout-p2996:latest \
    bash -c "cmake -B build -G Ninja && cmake --build build --target test_all_types"

# 3. 交互式 shell (用于调试)
docker run -it --rm -v $(pwd):/workspace -w /workspace \
    ghcr.io/ximicpp/typelayout-p2996:latest bash
```

### 故障排除

**问题**: "Docker image not found"
- **解决**: 先运行 Docker Build 工作流: GitHub Actions → "Build Docker Image" → Run workflow

**问题**: WSL 中 Docker 无法启动
- **解决**: 确保 Docker Desktop 已启用 WSL 2 后端

**问题**: 编译错误 "unknown argument: '-freflection'"
- **解决**: 确保使用 P2996 Docker 镜像，而非标准 Clang

**问题**: 测试运行时错误 "libc++.so.1: cannot open shared object file"
- **原因**: P2996 工具链的 libc++ 安装在 `/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu/`，不在标准库搜索路径中
- **解决**: 启动 Docker 时必须设置 `-e LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu`

**问题**: CMake 报错 "binary dir was already configured"
- **原因**: 在 Docker 容器内使用主机创建的 `build/` 目录，路径不匹配
- **解决**: 使用只读挂载 + 拷贝策略（见方式 1 命令），或先删除 `build/` 目录

## Implementation Details

### CompileString<N> 模板
编译时字符串操作的核心：
- `consteval` 构造函数
- `operator+` 连接
- `operator==` 比较
- `from_number()` 数字转字符串

### 递归签名生成
使用折叠表达式和索引序列生成所有字段签名：
```cpp
template<typename T, std::size_t... Indices>
consteval auto concatenate_field_signatures(std::index_sequence<Indices...>) noexcept {
    return (build_field_with_comma<T, Indices, (Indices == 0)>() + ...);
}
```

### 双哈希验证
```cpp
struct LayoutVerification {
    uint64_t fnv1a;   // FNV-1a 64位哈希
    uint64_t djb2;    // DJB2 64位哈希（独立算法）
    uint32_t length;  // 签名长度
};
```