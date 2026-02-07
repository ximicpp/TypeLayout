# Project Context

## Purpose
Boost.TypeLayout 是一个 C++26 header-only 库，使用 P2996 静态反射提供编译时内存布局分析和验证。通过两层签名系统（Layout / Definition）唯一标识类型的内存布局和结构。

**核心保证**: `Identical layout signature ⟹ Identical memory layout`（相同签名→相同内存布局；反之不一定成立，如数组 vs 散字段）

**投影关系**: `definition_match(T, U) ⟹ layout_match(T, U)`（反之不成立）

### 核心价值（V1/V2/V3）

| # | 承诺 | 形式化表达 | 说明 |
|---|------|-----------|------|
| V1 | 布局签名的**可靠性** | `layout_sig(T) == layout_sig(U) ⟹ memcmp-compatible(T, U)` | 签名相同→字节布局相同，方向正确且保守 |
| V2 | 结构签名的**精确性** | `def_sig(T) == def_sig(U) ⟹ T 和 U 的字段名、类型、层次完全一致` | 区分所有结构差异（名称、类型、继承、命名空间） |
| V3 | 两层的**投影关系** | `def_match(T, U) ⟹ layout_match(T, U)` | Definition 是 Layout 的细化（refinement），反之不成立 |

### 设计哲学：结构分析 vs 名义分析

TypeLayout 执行**结构分析（Structural Analysis）**而非名义分析（Nominal Analysis）。
两个不同名称的类型（`struct Point` 和 `struct Coord`），如果它们的字段名、类型、布局完全相同，
则 Definition 签名相同。签名**不包含类型自身的名称**——这是有意的设计选择，
因为 TypeLayout 关注的是"两个类型的结构是否等价"，而非"它们是否是同一个类型"。

### 两层签名使用场景指导

| 场景 | 推荐层 | 理由 |
|------|--------|------|
| 共享内存 / IPC | Layout | 只关心字节布局是否 memcpy 兼容 |
| 网络协议验证 | Layout | 只关心字节对齐和偏移 |
| 编译器 ABI 验证 | Layout | 验证二进制兼容性 |
| 序列化版本检查 | Definition | 需要检测字段名变更等结构变化 |
| API 兼容检查 | Definition | 关心语义级结构一致性 |
| ODR 违规检测 | Definition | 需要完整结构信息 |

## Tech Stack
- **语言**: C++26
- **反射**: P2996 静态反射 (`<experimental/meta>`)
- **编译器**: Bloomberg Clang P2996 fork
- **构建系统**: CMake
- **库类型**: Header-only
- **编译选项**: `-std=c++26 -freflection -freflection-latest -stdlib=libc++`

## Project Structure
```
TypeLayout/
├── include/boost/
│   ├── typelayout.hpp                 # 便捷头文件
│   └── typelayout/
│       ├── typelayout.hpp             # 入口头文件
│       └── core/
│           ├── config.hpp             # 平台检测、SignatureMode 枚举
│           ├── compile_string.hpp     # CompileString<N> 编译时字符串
│           ├── reflection_helpers.hpp # P2996 反射辅助、展平逻辑
│           ├── type_signature.hpp     # TypeSignature<T,Mode> 特化
│           └── signature.hpp          # 公共 API（4 个函数）
├── test/
│   └── test_two_layer.cpp             # 两层签名系统测试
├── example/
│   └── cross_platform_check.cpp       # 跨平台兼容性检查 demo
├── scripts/
│   └── compare_signatures.py          # 多平台签名对比工具
├── CMakeLists.txt
└── README.md
```

### 架构说明

- **Core Layer (`core/`)**: 两层签名引擎——Layout（字节身份）和 Definition（结构身份）

## Project Conventions

### Code Style
- 遵循 Boost 库代码风格
- 使用 `boost::typelayout` 命名空间
- 所有核心功能使用 `consteval` 实现编译时计算
- 使用 `[[nodiscard]]` 标记返回值重要的函数

### Architecture Patterns
- **Header-only 设计**: 所有实现在头文件中
- **编译时计算优先**: 零运行时开销
- **静态反射**: 使用 P2996 API 进行类型分析
- **递归类型签名**: 支持嵌套结构体和继承层次
- **两层签名**: Layout（展平字节身份）+ Definition（保留结构树）

### Testing Strategy
- **编译时测试**: 使用 `static_assert` 进行编译时测试（编译成功=测试通过）
- **类型覆盖**: 测试覆盖所有基本类型、复合类型、继承、位域等
- **跨平台验证**: 架构前缀 `[64-le]`/`[32-be]` 区分平台

### Git Workflow
- main 分支为稳定分支
- feature/* 分支用于新功能开发
- 提交信息使用描述性语言

## Core Components

### 1. 两层签名格式

**Layout 签名**（Layer 1）— 纯字节身份，展平所有层次，不含字段名：
```
[ARCH]record[s:SIZE,a:ALIGN]{@OFFSET:type_sig,...}
```

示例：
```cpp
struct Point { int32_t x, y; };
// Layout:     "[64-le]record[s:8,a:4]{@0:i32[s:4,a:4],@4:i32[s:4,a:4]}"
```

- 继承展平：基类字段以绝对偏移出现
- 组合展平：嵌套 struct 递归展开为叶字段
- 多态类型：`record[s:SIZE,a:ALIGN,vptr]{...}`
- union 成员：不展平，保持每个成员的完整类型签名

**Definition 签名**（Layer 2）— 保留结构树，含字段名和限定名：
```
[ARCH]record[s:SIZE,a:ALIGN]{~base<ns::Base>:record{...},@OFFSET[name]:type_sig,...}
```

示例：
```cpp
struct Point { int32_t x, y; };
// Definition: "[64-le]record[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}"
```

- 继承保留：`~base<QualifiedName>:record{...}`
- 虚继承：`~vbase<QualifiedName>:record{...}`
- 多态标记：`record[s:SIZE,a:ALIGN,polymorphic]{...}`
- 枚举含限定名：`enum<ns::Color>[s:1,a:1]<u8[s:1,a:1]>`

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
| 联合体 | `union{...}` | `union[s:8,a:8]{...}` |
| 记录类型 | `record{...}` | `record[s:8,a:4]{...}` |
| 位域 | `bits<width,type>` | `@4.2:bits<3,u8[s:1,a:1]>` |

### 3. 核心 API

4 个函数，全部 `consteval`，定义在 `signature.hpp`：

| 函数 | 说明 |
|------|------|
| `get_layout_signature<T>()` | 获取 Layout 签名（展平，无字段名） |
| `layout_signatures_match<T1, T2>()` | 比较两个类型的 Layout 签名是否相同 |
| `get_definition_signature<T>()` | 获取 Definition 签名（保留层次，含字段名） |
| `definition_signatures_match<T1, T2>()` | 比较两个类型的 Definition 签名是否相同 |

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
- 需要 Bloomberg Clang P2996 fork（唯一支持 P2996 的编译器）
- 必须是 header-only 以符合 Boost 库要求
- 所有分析必须在编译时完成（零运行时开销）
- 需要支持 IEEE 754 浮点数
- 指针大小编码在签名中（4或8字节）

## Known Limitations (已知限制)

### 设计性限制（非缺陷）

以下限制是有意的设计选择，不属于缺陷：

| 限制 | 原因 | 是否需要修复 |
|------|------|------------|
| 签名匹配是 `⟹` 而非 `⟺` | 保守设计——签名相同保证布局相同，但布局相同不保证签名相同（如 `int[3]` vs `int,int,int`） | 否 |
| 不包含类型自身名称 | 结构分析（Structural）而非名义分析（Nominal），关注结构等价而非类型身份 | 否 |
| union 成员不递归展平 | 展平会混合重叠成员导致签名碰撞，保持原子性是正确策略 | 否 |
| 数组不展开为散字段 | `int[3]` 与 `int a,b,c` 字节相同但语义不同，保留数组边界使签名更精确 | 否 |
| 仅支持 Bloomberg Clang P2996 fork | P2996 标准化后自然解除，非库本身的限制 | 否 |

### Constexpr Step Limits (编译器步数限制)

由于 `constexpr` 计算的固有限制，Bloomberg Clang P2996 fork 对复杂类型有默认步数限制。
当类型成员过多或签名字符串过长时，编译器可能报错：
```
constexpr evaluation hit maximum step limit; possible infinite loop?
```

**已验证的边界** (Structural 模式 - 无字段名):
| 测试用例 | 签名长度 | 每成员字符数 |
|----------|---------|-------------|
| 20字段结构体 | ~361 chars | ~18 |
| 40字段结构体 | ~717 chars | ~18 |
| 60字段结构体 | ~1077 chars | ~18 |
| 80字段结构体 | ~1437 chars | ~18 |
| 100字段结构体 | ~1797 chars | ~18 |

> **注**: 通过移除字段名，签名长度减少约 65%（从 ~5200 降至 ~1800 字符）。

**编译器参数建议**:
```bash
# 支持大型类型（100+字段）的编译选项
clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -fconstexpr-steps=3000000 \
    -I./include your_code.cpp

# 支持超大型类型的编译选项
clang++ ... -fconstexpr-steps=5000000 ...
```

**步数消耗的根本原因**:
1. `CompileString::operator+` 在每次连接时逐字符复制
2. 字段签名使用 fold expression 展开，产生 O(n²) 复制开销
3. 5000字符的签名 × 100次连接 = 大量循环迭代

**推荐配置**:
| 项目类型 | 最大结构体大小 | 建议步数限制 |
|----------|---------------|-------------|
| 小型项目 | <50 字段 | 默认 (不设置) |
| 中型项目 | 50-80 字段 | `-fconstexpr-steps=1500000` |
| 大型项目 | 80-100 字段 | `-fconstexpr-steps=3000000` |
| 超大型项目 | 100+ 字段 | `-fconstexpr-steps=5000000` |

**CMake 配置示例**:
```cmake
# 对于包含大型类型的项目
target_compile_options(your_target PRIVATE
    $<$<CXX_COMPILER_ID:Clang>:-fconstexpr-steps=3000000>
)
```

## Use Cases

### 1. 二进制协议验证
```cpp
struct NetworkHeader { uint32_t magic; uint64_t timestamp; };
// 编译时确保布局与预期一致
static_assert(get_layout_signature<NetworkHeader>() ==
    CompileString{"[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:u64[s:8,a:8]}"});
```

### 2. 共享内存验证
```cpp
// 确保两侧使用的类型具有相同内存布局
static_assert(layout_signatures_match<ShmWriter::Header, ShmReader::Header>(),
    "Shared memory header layout mismatch!");
```

### 3. 模板约束
```cpp
template<typename T>
    requires layout_signatures_match<T, ExpectedType>()
void process(const T& data) { ... }
```

### 4. 版本兼容检查
```cpp
// Definition 签名区分结构变更（字段名、继承关系）
static_assert(definition_signatures_match<ConfigV1, ConfigV2>(),
    "Config struct definition changed — update serialization logic");
```

## External Dependencies
- **Bloomberg Clang P2996**: 唯一支持 P2996 的编译器实现
- **Boost License**: 使用 Boost Software License 1.0

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

### 快速编译单个测试文件

无需使用 CMake，直接编译单个测试文件进行快速验证：

**Windows (通过 WSL + Docker)**:
```cmd
wsl -e bash -c "cd /mnt/g/workspace/TypeLayout && docker run --rm -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash -c 'clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ -I./include -o test_output test/test_complex_cases.cpp'"
```

**Linux/macOS (直接 Docker)**:
```bash
docker run --rm -v $(pwd):/workspace -w /workspace \
    ghcr.io/ximicpp/typelayout-p2996:latest \
    clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I./include -o test_output test/test_complex_cases.cpp
```

**运行测试可执行文件**:
```bash
# 需要设置 LD_LIBRARY_PATH 以找到 libc++
docker run --rm -v $(pwd):/workspace -w /workspace \
    -e LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu \
    ghcr.io/ximicpp/typelayout-p2996:latest \
    ./test_output
```

**编译+运行一体化命令 (Windows WSL)**:
```cmd
wsl -e bash -c "cd /mnt/g/workspace/TypeLayout && docker run --rm -v $(pwd):/workspace -w /workspace -e LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu ghcr.io/ximicpp/typelayout-p2996:latest bash -c 'clang++ -std=c++26 -freflection -freflection-latest -stdlib=libc++ -I./include -o test_output test/test_complex_cases.cpp && ./test_output'"
```

**编译选项说明**:
| 选项 | 说明 |
|------|------|
| `-std=c++26` | 使用 C++26 标准 |
| `-freflection` | 启用 P2996 静态反射 |
| `-freflection-latest` | 使用最新的反射 API |
| `-stdlib=libc++` | 使用 LLVM libc++ 标准库 |
| `-I./include` | 添加项目头文件路径 |

**清理编译产物**:
```bash
# Windows (通过 WSL)
wsl -e bash -c "cd /mnt/g/workspace/TypeLayout && rm -f test_output"

# Linux/macOS
rm -f test_output
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
- `skip_first()` 去除前导逗号（展平折叠表达式产物）

### 签名生成流程
1. **Layout 模式**：`layout_all_prefixed<T, 0>()` 递归展平基类和嵌套 struct，所有字段以逗号前缀形式累积，最终 `skip_first()` 去掉前导逗号
2. **Definition 模式**：`definition_content<T>()` 保留继承树和字段名，基类以 `~base<>` / `~vbase<>` 前缀出现
3. 两种模式共享 `TypeSignature<T, Mode>` 特化，基础类型签名两层通用
