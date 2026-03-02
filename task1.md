# 实现任务：Boost.TypeLayout 移除 introduces_vptr，信任可观测量

## 背景

Boost.TypeLayout 是一个基于 C++26 P2996 静态反射的库，用于在编译期生成类型的内存布局签名（layout signature）。签名的核心价值是准确反映类型的物理内存布局。

当前实现中存在一个 introduces_vptr 启发式函数，试图检测编译器注入的隐藏 vptr（虚表指针）并将其显式编码进签名。该启发式存在以下问题：

1. 只覆盖最简单的情况（单继承、无虚基类）
2. 多重继承时可能漏检第二个 vptr
3. 虚继承时完全无法处理 MSVC 的 vbptr
4. 增加了代码复杂度和 ABI 特定逻辑

## 核心洞察

vptr/vbptr 不需要显式建模。编译器注入的任何隐藏数据，必然会：

- 增大 sizeof(T)，或
- 推移成员的 offset_of

而 sizeof(T) + alignof(T) + 每个显式成员的 {offset, size, type} 已经在签名中了。隐藏数据的存在被这些可观测量间接但完整地编码。

验证：不可能存在两个物理布局不同的类型，却具有相同的 sizeof、alignof 和所有成员的 (offset, type)。因此删除 vptr 显式建模后，签名的唯一性和正确性不受影响。

## 具体要求

### 1. 删除的内容

- 删除 introduces_vptr 函数及其所有相关逻辑
- 删除 vptr 作为特殊字段类型的定义（如果有 field_kind::vptr 或类似枚举值）
- 删除所有与 vptr 检测相关的 ABI 判断代码
- 删除相关的测试用例中对 vptr 字段的断言（如果测试是验证 vptr 检测行为的），改为验证新签名格式

### 2. 保留的内容

- sizeof(T) 和 alignof(T) 编码进签名（已有行为，不变）
- 每个显式成员的 {offset_of, size_of, type_signature} 编码进签名（已有行为，不变）
- 基类子对象的递归签名处理（已有行为，不变）
- padding 的处理逻辑不变（padding 是成员之间或末尾的间隙，由 sizeof 和 offset 自然体现）

### 3. 签名格式变化

修改前（当前方案）：

    struct A { virtual void f(); int x; };
    → "record[s:16,a:8]{@0:vptr[s:8,a:8],@8:i32[s:4,a:4]}"

修改后（新方案）：

    struct A { virtual void f(); int x; };
    → "record[s:16,a:8]{@8:i32[s:4,a:4]}"

offset=8 + sizeof=16 已经隐含了前 8 字节被隐藏数据占据。签名仍然唯一标识该布局。

### 4. 各场景预期签名

场景 1：简单多态

    struct A { virtual void f(); int x; };
    // 实际布局 (LP64): [vptr(8)][x(4)][pad(4)]  sizeof=16
    // 签名: record[s:16,a:8]{@8:i32[s:4,a:4]}

场景 2：多重继承

    struct A { virtual void f(); };
    struct B { virtual void g(); };
    struct C : A, B { int z; };
    // 实际布局 (Itanium LP64): [vptr_A(8)][vptr_B(8)][z(4)][pad(4)]  sizeof=24
    // 签名: record[s:24,a:8]{@16:i32[s:4,a:4]}

场景 3：虚继承（Itanium）

    struct V { int v; };
    struct A : virtual V { int a; };
    // 实际布局: [vptr(8)][a(4)][pad(4)][v(4)][pad(4)]  sizeof=24
    // 签名: record[s:24,a:8]{@8:i32[s:4,a:4],@16:i32[s:4,a:4]}

场景 4：非多态（不受影响）

    struct Plain { int x; double y; };
    // 实际布局: [x(4)][pad(4)][y(8)]  sizeof=16
    // 签名: record[s:16,a:8]{@0:i32[s:4,a:4],@8:f64[s:8,a:8]}

场景 5：继承链

    struct Base { virtual void f(); int b; };
    struct Derived : Base { double d; };
    // 实际布局 (LP64): [vptr(8)][b(4)][pad(4)][d(8)]  sizeof=24
    // 签名: record[s:24,a:8]{@8:i32[s:4,a:4],@16:f64[s:8,a:8]}

### 5. 实现步骤

1. 在代码库中搜索所有 introduces_vptr 的定义和调用点
2. 搜索 vptr 相关的字段类型定义（枚举值、字符串常量等）
3. 删除上述所有代码
4. 在构建字段列表（build_fields 或类似函数）中，移除 vptr 注入逻辑，只保留对显式成员的反射遍历
5. 更新签名生成逻辑，确保不再输出 vptr 字段
6. 更新所有测试用例，将包含 vptr 的预期签名改为不含 vptr 的版本
7. 编译并运行测试，确保所有签名仍然正确且唯一

### 6. 验收标准

- introduces_vptr 函数不再存在于代码库中
- 签名中不再出现 "vptr" 字样
- 所有原有测试（调整预期值后）通过
- 多态类型的签名仍然唯一——不同布局产生不同签名
- 非多态类型的签名完全不受影响

### 7. 不要做的事情

- 不要引入 "opaque" 或 "hole" 概念来替代 vptr
- 不要引入分层/组合架构
- 不要改变签名中 sizeof、alignof、offset 的编码方式
- 不要改变非多态类型的任何行为
- 保持方案精简，这是一个纯删除性质的重构