# Boost.TypeLayout API Reference

This document provides a comprehensive reference for all public APIs in Boost.TypeLayout.

## Headers Overview

| Header | Description |
|--------|-------------|
| `<boost/typelayout.hpp>` | Core layer only - layout signature generation |
| `<boost/typelayout/typelayout_util.hpp>` | Utility layer - serialization safety analysis |
| `<boost/typelayout/typelayout_all.hpp>` | Complete library - both core and utility |

## Table of Contents

1. [Core Layer](#core-layer)
   - [Signature Generation](#signature-generation)
   - [Hash Functions](#hash-functions)
   - [Comparison Functions](#comparison-functions)
   - [Core Concepts](#core-concepts)
2. [Utility Layer](#utility-layer)
   - [Platform Set](#platform-set)
   - [Serialization Analysis](#serialization-analysis)
   - [Utility Concepts](#utility-concepts)
3. [Collision Detection](#collision-detection)
4. [Utility Classes](#utility-classes)
5. [Macros](#macros)
6. [Implementation Details](#implementation-details)

---

# Core Layer

**Header**: `<boost/typelayout.hpp>` or `<boost/typelayout/core/*.hpp>`

The core layer provides pure memory layout analysis capabilities without any serialization policy dependencies.

## Signature Generation

### get_layout_signature<T>()

```cpp
template <typename T>
[[nodiscard]] consteval auto get_layout_signature() noexcept;
```

**Description**: Generates a compile-time layout signature string for type `T`.

**Parameters**:
- `T`: The type to analyze

**Returns**: A `CompileString` containing the complete layout signature including platform prefix

**Example**:
```cpp
#include <boost/typelayout.hpp>

struct Point { int32_t x, y; };
constexpr auto sig = boost::typelayout::get_layout_signature<Point>();
// Result: "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}"
```

**Notes**: 
- All analysis happens at compile time with zero runtime overhead
- Platform prefix format: `[bits-endian]` (e.g., `[64-le]`, `[32-be]`)

---

### get_layout_signature_cstr<T>()

```cpp
template <typename T>
[[nodiscard]] consteval const char* get_layout_signature_cstr() noexcept;
```

**Description**: Returns the layout signature as a null-terminated C-style string.

**Parameters**:
- `T`: The type to analyze

**Returns**: `const char*` pointing to the signature string

**Example**:
```cpp
std::cout << "Layout: " << get_layout_signature_cstr<Point>() << std::endl;
```

**Notes**: Suitable for runtime output and logging.

---

## Hash Functions

### get_layout_hash<T>()

```cpp
template <typename T>
[[nodiscard]] consteval uint64_t get_layout_hash() noexcept;
```

**Description**: Generates a 64-bit FNV-1a hash of the layout signature.

**Parameters**:
- `T`: The type to hash

**Returns**: `uint64_t` hash value

**Example**:
```cpp
constexpr uint64_t point_hash = get_layout_hash<Point>();
static_assert(point_hash == get_layout_hash<Vec2>(), "Must have same layout");
```

**Notes**: 
- Uses FNV-1a algorithm for fast, consistent hashing
- Collision probability: ~1/2^64

---

### get_layout_verification<T>()

```cpp
template <typename T>
[[nodiscard]] consteval LayoutVerification get_layout_verification() noexcept;
```

**Description**: Generates dual-hash verification using both FNV-1a and DJB2 algorithms.

**Parameters**:
- `T`: The type to verify

**Returns**: `LayoutVerification` struct containing:
- `fnv1a`: FNV-1a 64-bit hash
- `djb2`: DJB2 64-bit hash
- `length`: Signature string length

**Example**:
```cpp
constexpr auto verification = get_layout_verification<Point>();
// verification.fnv1a = FNV-1a hash
// verification.djb2 = DJB2 hash
// verification.length = signature length
```

**Notes**: 
- Provides ~2^128 collision resistance
- Recommended for high-reliability applications

---

## Comparison Functions

### signatures_match<T, U>()

```cpp
template <typename T, typename U>
[[nodiscard]] consteval bool signatures_match() noexcept;
```

**Description**: Checks if two types have identical layout signatures.

**Parameters**:
- `T`, `U`: Types to compare

**Returns**: `true` if layouts are identical, `false` otherwise

**Example**:
```cpp
struct Point { int32_t x, y; };
struct Vec2 { int32_t x, y; };
static_assert(signatures_match<Point, Vec2>(), "Same layout");
```

---

### hashes_match<T, U>()

```cpp
template <typename T, typename U>
[[nodiscard]] consteval bool hashes_match() noexcept;
```

**Description**: Checks if two types have the same layout hash.

**Parameters**:
- `T`, `U`: Types to compare

**Returns**: `true` if hashes match, `false` otherwise

**Example**:
```cpp
static_assert(hashes_match<Point, Vec2>(), "Hash must match");
```

---

### verifications_match<T, U>()

```cpp
template <typename T, typename U>  
[[nodiscard]] consteval bool verifications_match() noexcept;
```

**Description**: Checks if two types have matching dual-hash verifications.

**Parameters**:
- `T`, `U`: Types to compare

**Returns**: `true` if both hash pairs match, `false` otherwise

**Example**:
```cpp
static_assert(verifications_match<Point, Vec2>(), "Verification must match");
```

---

## Serializability Analysis

### is_serializable_v<T, PlatformSet>

```cpp
template <typename T, PlatformSet Platform = PlatformSet::current()>
inline constexpr bool is_serializable_v;
```

**Description**: Determines if a type is safe for binary serialization across platforms.

**Parameters**:
- `T`: Type to analyze
- `Platform`: Target platform configuration (default: current platform)

**Returns**: `true` if serializable, `false` if contains elements unsafe for serialization

**Non-serializable Elements**:
- `long double` (size varies: 8, 12, or 16 bytes)
- `wchar_t` (size varies: 2 or 4 bytes)
- `long` / `unsigned long` (size varies: 4 or 8 bytes)
- Bit-fields (packing is compiler/ABI dependent)
- Pointers (not meaningful across processes)
- Unions with non-trivial members

**Example**:
```cpp
struct SafeType { int32_t x; float y; };
struct UnsafeType { long double x; wchar_t name[16]; };

static_assert(is_serializable_v<SafeType, PlatformSet::bits64_le()>, "Should be serializable");
static_assert(!is_serializable_v<UnsafeType, PlatformSet::current()>, "Contains unsafe types");
```

---

## Template Constraints and Concepts

### Serializable<T>

```cpp
template<typename T>
concept Serializable = is_serializable_v<T, PlatformSet::current()>;
```

**Description**: Concept for types safe for binary serialization.

**Example**:
```cpp
template<Serializable T>
void serialize(const T& obj) {
    // Safe to serialize across platforms
}
```

---

### LayoutMatch<T, Signature>

```cpp
template<typename T, CompileString Signature>
concept LayoutMatch = (get_layout_signature<T>() == Signature);
```

**Description**: Concept requiring exact layout signature match.

**Parameters**:
- `T`: Type to constrain
- `Signature`: Expected layout signature string

**Example**:
```cpp
template<typename Vector2D>
    requires LayoutMatch<Vector2D, "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}">
class GeometryProcessor {
    // Can assume specific Vector2D layout
};
```

---

### LayoutHashMatch<T, Hash>

```cpp
template<typename T, uint64_t Hash>
concept LayoutHashMatch = (get_layout_hash<T>() == Hash);
```

**Description**: Concept requiring exact layout hash match.

**Parameters**:
- `T`: Type to constrain
- `Hash`: Expected hash value

**Example**:
```cpp
constexpr uint64_t POINT_HASH = get_layout_hash<Point>();

template<typename T>
    requires LayoutHashMatch<T, POINT_HASH>
void process_point_data(const T& data) {
    // Hash-verified compatible layout
}
```

---

## Collision Detection

### no_hash_collision<Types...>()

```cpp
template <typename... Types>
[[nodiscard]] consteval bool no_hash_collision() noexcept;
```

**Description**: Verifies no hash collisions exist in a set of types.

**Parameters**:
- `Types...`: Variable number of types to check

**Returns**: `true` if all hashes are unique, `false` if collision found

**Example**:
```cpp
static_assert(no_hash_collision<Point, Player, Enemy, Item>(), 
              "No hash collisions in type library");
```

---

### no_verification_collision<Types...>()

```cpp
template <typename... Types>  
[[nodiscard]] consteval bool no_verification_collision() noexcept;
```

**Description**: Verifies no dual-hash collisions in a type set.

**Returns**: `true` if all verification pairs are unique

**Example**:
```cpp
static_assert(no_verification_collision<Point, Player, Enemy, Item>(),
              "No verification collisions in type library");
```

---

## Utility Classes

### CompileString<N>

```cpp
template <size_t N>
class CompileString;
```

**Description**: Compile-time string class for building signatures.

**Key Methods**:
- `consteval CompileString(const char (&str)[N])`
- `consteval auto operator+(const CompileString<M>& other) const`
- `consteval bool operator==(const CompileString<M>& other) const`
- `consteval const char* data() const noexcept`
- `consteval size_t size() const noexcept`

**Static Methods**:
- `template<size_t BufferSize> static consteval auto from_number(auto value)`

**Example**:
```cpp
consteval auto build_signature() {
    return CompileString{"struct[s:"} + 
           CompileString<32>::from_number(sizeof(Point)) +
           CompileString{"]"};
}
```

---

### TypeSignature<T>

```cpp
template <typename T>
struct TypeSignature {
    static consteval auto calculate() noexcept;
};
```

**Description**: Customization point for specialized type signatures.

**Specializations Provided**:
- Arithmetic types (`int32_t`, `float`, `double`, etc.)
- Pointer types (`T*`, `void*`)
- Array types (`T[N]`)
- Struct/class types (via reflection)
- Union types
- Enum types

**Custom Specialization Example**:
```cpp
// User-defined specialization for custom string class
template <>
struct TypeSignature<MyString> {
    static consteval auto calculate() noexcept {
        return CompileString{"mystring[s:"} +
               CompileString<32>::from_number(sizeof(MyString)) +
               CompileString{",a:"} +
               CompileString<32>::from_number(alignof(MyString)) +
               CompileString{"]"};
    }
};
```

---

## Macros

### TYPELAYOUT_BIND(Type, ExpectedSignature)

```cpp
#define TYPELAYOUT_BIND(Type, ExpectedSig) \
    static_assert(::boost::typelayout::get_layout_signature<Type>() == ExpectedSig, \
                  "Layout signature mismatch for " #Type)
```

**Description**: Binds a type to an expected layout signature at compile time.

**Parameters**:
- `Type`: The type to bind
- `ExpectedSignature`: String literal of expected signature

**Example**:
```cpp
TYPELAYOUT_BIND(Point, "[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}");
```

**Error Message**: If layout changes, compilation fails with clear diagnostic showing expected vs actual signature.

---

## Implementation Details

### Platform Detection

The library automatically detects:
- **Architecture**: x86_64, ARM64, etc.
- **Endianness**: Little-endian (`le`) or big-endian (`be`)
- **Pointer Size**: 32-bit or 64-bit

Platform prefixes:
- `[64-le]`: 64-bit little-endian (most common)
- `[64-be]`: 64-bit big-endian
- `[32-le]`: 32-bit little-endian
- `[32-be]`: 32-bit big-endian

### Reflection Usage

The library uses C++26 static reflection (P2996) operators:
- `^^T`: Get reflection of type `T`
- `[:r:]`: Splice reflection back to type
- `members_of(^^T)`: Get member reflections
- `name_of(member)`: Get member name
- `type_of(member)`: Get member type
- `offset_of(member)`: Get member offset

### Hash Algorithms

**FNV-1a (64-bit)**:
```
hash = 14695981039346656037ULL  // FNV offset basis
for each byte:
    hash ^= byte
    hash *= 1099511628211ULL    // FNV prime
```

**DJB2**:
```
hash = 5381
for each byte:
    hash = hash * 33 + byte
```

### Memory Layout

Type signatures capture:
- **Size**: Total byte size (`sizeof(T)`)
- **Alignment**: Required alignment (`alignof(T)`)
- **Field Offsets**: Exact byte offset of each member
- **Field Types**: Recursive signatures for nested types
- **Padding**: Implicit via offset differences

### Error Handling

All functions are `noexcept` and `consteval`. Errors result in:
- Compilation failure for invalid types
- Clear diagnostic messages via `static_assert`
- No runtime exceptions or error codes

### Performance

- **Compile Time**: Signature generation is linear in type complexity
- **Runtime**: Zero overhead - all computation at compile time
- **Memory**: No runtime storage - signatures embedded as string literals
- **Binary Size**: Minimal impact - only string literals included if used

---

## Deprecated APIs

The following APIs are deprecated and will be removed in a future version:

| Deprecated | Replacement |
|------------|-------------|
| `is_portable<T>()` | `is_serializable_v<T>` |
| `<boost/typelayout/signature.hpp>` | `<boost/typelayout.hpp>` |
| `<boost/typelayout/portability.hpp>` | `<boost/typelayout/typelayout_util.hpp>` |

---

## Version History

- **v1.0**: Initial implementation with P2996 reflection
- **v1.1**: Added dual-hash verification system
- **v1.2**: Enhanced portability analysis
- **v1.3**: Boost compliance and standardization
- **v2.0**: Core/Util architectural split

## See Also

- [Quick Start Guide](quickstart.md)
- [Technical Overview](technical_overview.md)
- [C++26 Reflection Proposal (P2996)](https://wg21.link/P2996)
- [Boost C++ Libraries](https://www.boost.org/)