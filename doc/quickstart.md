# Boost.TypeLayout Quick Start Guide

This guide will walk you through everything you need to know to get started with Boost.TypeLayout, from installation to your first successful layout verification.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installation](#installation)
3. [Your First Example](#your-first-example)
4. [Understanding Signatures](#understanding-signatures)
5. [Portability Checking](#portability-checking)
6. [Template Constraints](#template-constraints)
7. [Common Use Cases](#common-use-cases)
8. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Required Compiler

Boost.TypeLayout requires a C++26 compiler with P2996 reflection support. Currently, only Bloomberg's Clang fork provides this:

```bash
# Clone Bloomberg's P2996 implementation
git clone https://github.com/bloomberg/clang-p2996.git
cd clang-p2996

# Build according to Bloomberg's instructions
# This typically involves:
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/clang-p2996 \
      -DLLVM_ENABLE_PROJECTS="clang" \
      ../llvm
ninja install
```

### System Requirements

- **C++26 Standard Library**: Compatible implementation
- **CMake**: 3.16 or later (for building examples)
- **Platform**: Linux, macOS, or Windows with appropriate development tools

---

## Installation

### Header-Only Library

Boost.TypeLayout is header-only. Choose your include based on needs:

```cpp
// Core layer only - Layout signatures and verification
#include <boost/typelayout/typelayout.hpp>

// Utility layer - Adds serialization safety checking (includes core)
#include <boost/typelayout/typelayout_util.hpp>

// All features (convenience header)
#include <boost/typelayout/typelayout_all.hpp>
```

**Layered Architecture:**
- **Core Layer**: Layout signature generation, hash computation, `LayoutCompatible`/`LayoutMatch` concepts
- **Utility Layer**: Serialization safety checking, platform sets, `Serializable`/`ZeroCopyTransmittable` concepts

### Clone from GitHub

```bash
git clone https://github.com/ximicpp/TypeLayout.git
cd TypeLayout
```

### Using CMake (Recommended)

Create a `CMakeLists.txt` in your project:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_project CXX)

# Set C++26 and reflection flags
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -freflection")

# Add TypeLayout include path
include_directories(/path/to/TypeLayout/include)

add_executable(my_app main.cpp)
```

### Manual Compilation

```bash
clang++ -std=c++26 -freflection -I /path/to/TypeLayout/include main.cpp -o my_app
```

---

## Your First Example

Let's start with a simple example to verify that everything works:

### Step 1: Create main.cpp

```cpp
#include <iostream>
#include <cstdint>
#include <boost/typelayout/typelayout.hpp>

using namespace boost::typelayout;

// Define a simple structure
struct Point {
    int32_t x;
    int32_t y;
};

int main() {
    // Generate layout signature
    constexpr auto signature = get_layout_signature<Point>();
    
    // Display the signature
    std::cout << "Point layout signature: " 
              << get_layout_signature_cstr<Point>() << std::endl;
    
    // Check if Point is serializable across platforms
    constexpr bool serializable = is_serializable_v<Point, PlatformSet::current()>;
    std::cout << "Point is serializable: " << (serializable ? "yes" : "no") << std::endl;
    
    // Generate layout hash for quick comparisons
    constexpr uint64_t hash = get_layout_hash<Point>();
    std::cout << "Point layout hash: 0x" << std::hex << hash << std::dec << std::endl;
    
    std::cout << "TypeLayout is working correctly!" << std::endl;
    return 0;
}
```

### Step 2: Compile and Run

```bash
clang++ -std=c++26 -freflection -I include main.cpp -o first_example
./first_example
```

**Expected Output:**
```
Point layout signature: [64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}
Point is serializable: yes
Point layout hash: 0xa1b2c3d4e5f60789
TypeLayout is working correctly!
```

---

## Understanding Signatures

Layout signatures provide a complete description of memory layout. Let's break down the format:

### Signature Structure

```
[platform]type[size:N,align:N]{field_list}
```

### Example Breakdown

For `struct Point { int32_t x, y; }`:
```
[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}
```

- `[64-le]`: Platform (64-bit little-endian)
- `struct`: Type category
- `[s:8,a:4]`: Size 8 bytes, alignment 4 bytes
- `@0[x]:i32[s:4,a:4]`: Field `x` at offset 0, type `int32_t`
- `@4[y]:i32[s:4,a:4]`: Field `y` at offset 4, type `int32_t`

### More Complex Example

```cpp
struct Player {
    uint64_t id;        // 8 bytes, aligned to 8
    char name[32];      // 32 bytes, aligned to 1
    Point position;     // 8 bytes, aligned to 4
    float health;       // 4 bytes, aligned to 4
};
// Total: 56 bytes (with padding)
```

Signature:
```
[64-le]struct[s:56,a:8]{
  @0[id]:u64[s:8,a:8],
  @8[name]:bytes[s:32,a:1],
  @40[position]:struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]},
  @48[health]:f32[s:4,a:4]
}
```

---

## Serializability Checking

One of TypeLayout's key features is detecting types that may cause issues in binary serialization across platforms:

### Serializable Types

```cpp
struct GoodForSerialization {
    int32_t id;
    float value;
    char data[16];
};

static_assert(is_serializable_v<GoodForSerialization, PlatformSet::bits64_le()>, 
              "Safe for cross-platform serialization");
```

### Non-Serializable Types

```cpp
struct ProblematicForSerialization {
    long double precision;    // Size varies: 8, 12, or 16 bytes
    wchar_t name[32];         // Size varies: 2 or 4 bytes per char
    struct { int flag : 1; }; // Bit-field packing varies
};

static_assert(!is_serializable_v<ProblematicForSerialization, PlatformSet::current()>,
              "Will cause issues across platforms");
```

### Using Serializable Concept

```cpp
template<Serializable T>
void safe_serialize(const T& obj, std::ostream& out) {
    // Guaranteed to work consistently across platforms
    out.write(reinterpret_cast<const char*>(&obj), sizeof(T));
}
```

---

## Template Constraints

TypeLayout enables powerful compile-time constraints based on memory layout:

### Layout Signature Constraints

```cpp
// Require exact layout match
template<typename Vector2D>
    requires LayoutMatch<Vector2D, "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}">
void process_vector2d(const Vector2D& v) {
    // Can safely assume Vector2D has Point-compatible layout
    std::cout << "Processing 2D vector at (" << v.x << ", " << v.y << ")\n";
}

struct Point { int32_t x, y; };
struct Vec2  { int32_t x, y; };
struct Vec3  { int32_t x, y, z; }; // Different layout

Point p{10, 20};
Vec2  v{30, 40};
Vec3  v3{50, 60, 70};

process_vector2d(p);   // ✓ Compiles
process_vector2d(v);   // ✓ Compiles  
// process_vector2d(v3);  // ✗ Compilation error
```

### Hash-based Constraints

```cpp
constexpr uint64_t POINT_HASH = get_layout_hash<Point>();

template<typename T>
    requires LayoutHashMatch<T, POINT_HASH>
void fast_point_processing(const T& point) {
    // Layout verified by hash at compile time
    // Faster than string comparison
}
```

### Layout Binding (Recommended)

```cpp
// Bind type to expected signature - fails compilation if layout changes
TYPELAYOUT_BIND(Point, "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}");

// If someone changes Point to:
// struct Point { float x, y; };  // Now float instead of int32_t
// Compilation will fail with clear error message
```

---

## Common Use Cases

### 1. Binary File Format Verification

```cpp
struct FileHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t file_size;
    uint32_t checksum;
};

// Ensure header layout never changes
TYPELAYOUT_BIND(FileHeader, 
    "[64-le]struct[s:20,a:8]{@0[magic]:u32[s:4,a:4],@4[version]:u32[s:4,a:4],"
    "@8[file_size]:u64[s:8,a:8],@16[checksum]:u32[s:4,a:4]}");

void read_file(const std::string& filename) {
    // Safe to read directly into struct - layout is verified
    FileHeader header;
    std::ifstream file(filename, std::ios::binary);
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
}
```

### 2. Network Protocol Definition

```cpp
struct NetworkMessage {
    uint16_t message_type;
    uint16_t payload_length;
    uint64_t timestamp;
    // Variable payload follows...
} __attribute__((packed));

// Verify consistent layout across all platforms
static_assert(is_serializable_v<NetworkMessage, PlatformSet::bits64_le()>, 
              "Network message must be serializable");
TYPELAYOUT_BIND(NetworkMessage, 
    "[64-le]struct[s:12,a:1]{@0[message_type]:u16[s:2,a:1],"
    "@2[payload_length]:u16[s:2,a:1],@4[timestamp]:u64[s:8,a:1]}");
```

### 3. Template Library Safety

```cpp
// Ensure all Vector2D-like types have compatible layout
template<typename T>
class GeometryEngine {
    static_assert(sizeof(T) == 8, "Must be 8 bytes");
    static_assert(alignof(T) == 4, "Must be 4-byte aligned");
    
    // Better: Use TypeLayout for precise verification
    static constexpr auto EXPECTED_LAYOUT = 
        "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}";
    static_assert(get_layout_signature<T>() == EXPECTED_LAYOUT,
                  "Type must have Point-compatible layout");

public:
    void process(const T& point) {
        // Safe to assume specific memory layout
    }
};
```

### 4. Serialization Library Integration

```cpp
template<typename T>
class BinarySerializer {
public:
    static std::vector<uint8_t> serialize(const T& obj) {
        if constexpr (is_serializable_v<T, PlatformSet::current()>) {
            // Direct binary copy for serializable types
            std::vector<uint8_t> data(sizeof(T));
            std::memcpy(data.data(), &obj, sizeof(T));
            
            // Store layout signature for verification
            auto signature = get_layout_signature_cstr<T>();
            data.insert(data.begin(), signature, signature + std::strlen(signature) + 1);
            return data;
        } else {
            // Use field-by-field serialization for non-portable types
            return serialize_by_reflection(obj);
        }
    }
};
```

---

## Troubleshooting

### Common Compilation Errors

**Error**: `use of undeclared identifier 'members_of'`
**Solution**: Ensure you're using Bloomberg's Clang with `-freflection` flag:
```bash
clang++ -std=c++26 -freflection your_code.cpp
```

**Error**: `static_assert failed: Layout signature mismatch`
**Solution**: The type layout has changed. Update either the type or the expected signature in `TYPELAYOUT_BIND`.

**Error**: `no member named 'get_layout_signature' in namespace 'boost::typelayout'`
**Solution**: Make sure you're including the correct header:
```cpp
#include <boost/typelayout/typelayout.hpp>
using namespace boost::typelayout;
```

### Performance Issues

**Slow Compilation**: Large types with many fields can slow compilation. Consider:
- Breaking large types into smaller components
- Using hash-based comparisons instead of signature strings
- Limiting the number of types in collision detection

### Platform-Specific Issues

**Wrong Platform Prefix**: If you see `[32-le]` on a 64-bit system:
- Check your compiler target architecture
- Ensure you're not cross-compiling unintentionally

**Unexpected Signatures**: If signatures don't match expectations:
- Check for compiler-specific padding differences
- Verify you're using the same compiler flags across platforms
- Consider adding `__attribute__((packed))` for precise control

### Debugging Tips

**Print Actual Signatures**:
```cpp
#include <iostream>
std::cout << "Actual signature: " << get_layout_signature_cstr<MyType>() << std::endl;
```

**Check Individual Components**:
```cpp
std::cout << "Size: " << sizeof(MyType) << std::endl;
std::cout << "Alignment: " << alignof(MyType) << std::endl;
std::cout << "Serializable: " << is_serializable_v<MyType, PlatformSet::current()> << std::endl;
```

**Use Static Assertions for Debugging**:
```cpp
static_assert(sizeof(MyType) == 16, "Expected 16 bytes");
static_assert(alignof(MyType) == 8, "Expected 8-byte alignment");
static_assert(is_serializable_v<MyType, PlatformSet::current()>, "Should be serializable");
```

---

## Next Steps

1. **Read the [API Reference](api_reference.md)** for complete function documentation
2. **Explore the examples** in `libs/typelayout/example/`
3. **Run the test suite** in `libs/typelayout/test/`
4. **Check out the architecture guide** for implementation details

## Getting Help

- **GitHub Issues**: [https://github.com/ximicpp/TypeLayout/issues](https://github.com/ximicpp/TypeLayout/issues)
- **Discussions**: Use GitHub Discussions for questions and community support
- **Documentation**: Full API reference and guides in the `doc/` directory

---

*You're now ready to use Boost.TypeLayout for compile-time layout verification and ABI safety in your projects!*