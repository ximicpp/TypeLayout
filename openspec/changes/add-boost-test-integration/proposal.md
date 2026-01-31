# Change: 添加 Boost.Test 运行时测试集成

## Why
Boost 社区要求库必须包含 Boost.Test 集成的运行时测试套件。当前 TypeLayout 仅有编译时 `static_assert` 测试，缺少运行时测试框架，这是**阻塞 Boost 提交的关键问题**。

## What Changes
1. 添加 Boost.Test 依赖配置
2. 创建运行时测试套件结构
3. 将部分编译时测试转换为运行时验证
4. 添加额外的运行时特定测试场景

## 详细设计

### 1. 测试目录结构

```
test/
├── Jamfile.v2              # B2 测试配置
├── CMakeLists.txt          # CMake 测试配置
├── test_primitives.cpp     # 原始类型测试
├── test_structs.cpp        # 结构体测试
├── test_inheritance.cpp    # 继承测试
├── test_bitfields.cpp      # 位域测试
├── test_portability.cpp    # 可移植性测试
├── test_hash.cpp           # 哈希验证测试
└── test_concepts.cpp       # Concepts 测试
```

### 2. 测试类型

| 测试类型 | 说明 | 框架 |
|----------|------|------|
| 编译时测试 | 类型签名正确性 | `static_assert` (保留) |
| 运行时测试 | 签名字符串比较、哈希验证 | Boost.Test |
| 运行时测试 | 跨类型布局兼容性 | Boost.Test |

### 3. 示例测试代码

```cpp
#define BOOST_TEST_MODULE TypeLayout Tests
#include <boost/test/unit_test.hpp>
#include <boost/typelayout.hpp>

BOOST_AUTO_TEST_SUITE(PrimitiveTypes)

BOOST_AUTO_TEST_CASE(int32_signature) {
    constexpr auto sig = boost::typelayout::get_layout_signature<int32_t>();
    BOOST_CHECK_EQUAL(sig.c_str(), "[64-le]i32[s:4,a:4]");
}

BOOST_AUTO_TEST_CASE(hash_consistency) {
    constexpr auto hash1 = boost::typelayout::get_layout_hash<int32_t>();
    constexpr auto hash2 = boost::typelayout::get_layout_hash<int32_t>();
    BOOST_CHECK_EQUAL(hash1, hash2);
}

BOOST_AUTO_TEST_SUITE_END()
```

### 4. 构建系统集成

**B2 (Jamfile.v2)**:
```jam
import testing ;

project typelayout/test
    : requirements
        <library>/boost/test//boost_unit_test_framework
    ;

run test_primitives.cpp ;
run test_structs.cpp ;
run test_inheritance.cpp ;
run test_bitfields.cpp ;
run test_portability.cpp ;
run test_hash.cpp ;
run test_concepts.cpp ;
```

**CMake**:
```cmake
find_package(Boost REQUIRED COMPONENTS unit_test_framework)

add_executable(test_primitives test_primitives.cpp)
target_link_libraries(test_primitives Boost::unit_test_framework)
add_test(NAME primitives COMMAND test_primitives)
```

## Impact
- 满足 Boost 库测试要求
- 提供运行时验证能力
- 不影响现有编译时测试
