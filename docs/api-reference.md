# TypeLayout API Reference

Complete reference for all public symbols in Boost.TypeLayout.

**Header**: `<boost/typelayout/typelayout.hpp>` (umbrella) or individual headers as noted.

**Namespace**: `boost::typelayout` unless otherwise specified.

---

## Core Functions

### `get_layout_signature<T>()`

```cpp
// Header: <boost/typelayout/signature.hpp>
template <typename T>
[[nodiscard]] consteval auto get_layout_signature() noexcept;
```

Returns a `FixedString` containing the **Layout signature** of `T`.

Layout signatures capture pure byte identity: field types, sizes, alignments,
and offsets. Field names and inheritance structure are erased (flattened).

**Preconditions**: `T` must be a complete, reflectable type. The following are
not supported and will produce a compile error:
- `void`
- Unbounded arrays (`T[]`)
- Bare function types (`void(int)`)

**Example**:
```cpp
struct Msg { uint32_t id; double value; };
constexpr auto sig = get_layout_signature<Msg>();
// sig == "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:f64[s:8,a:8]}"
```

---

### `get_definition_signature<T>()`

```cpp
// Header: <boost/typelayout/signature.hpp>
template <typename T>
[[nodiscard]] consteval auto get_definition_signature() noexcept;
```

Returns a `FixedString` containing the **Definition signature** of `T`.

Definition signatures preserve full type structure: field names, inheritance
hierarchy (base classes with qualified names), virtual bases, and polymorphic
markers. This is a strict refinement of the Layout layer.

**Preconditions**: Same as `get_layout_signature`.

**Example**:
```cpp
struct Base { int32_t x; };
struct Derived : Base { double y; };
constexpr auto sig = get_definition_signature<Derived>();
// sig == "[64-le]record[s:16,a:8]{~base<Base>:record[...]{@0[x]:i32[...]},@8[y]:f64[...]}"
```

---

### `layout_signatures_match<T, U>()`

```cpp
// Header: <boost/typelayout/signature.hpp>
template <typename T1, typename T2>
[[nodiscard]] consteval bool layout_signatures_match() noexcept;
```

Returns `true` if `T1` and `T2` have identical Layout signatures.

A `true` result guarantees that `T1` and `T2` are `memcpy`-compatible
(identical field offsets, sizes, and alignments).

**Example**:
```cpp
struct A { int32_t x; int32_t y; };
struct B : Base { int32_t y; };  // flattens to same layout as A
static_assert(layout_signatures_match<A, B>());
```

---

### `definition_signatures_match<T, U>()`

```cpp
// Header: <boost/typelayout/signature.hpp>
template <typename T1, typename T2>
[[nodiscard]] consteval bool definition_signatures_match() noexcept;
```

Returns `true` if `T1` and `T2` have identical Definition signatures.

**Projection property**: `definition_signatures_match<T, U>()` implies
`layout_signatures_match<T, U>()`, but not the converse.

**Example**:
```cpp
struct A { int32_t x; int32_t y; };
struct B { int32_t x; int32_t y; };
static_assert(definition_signatures_match<A, B>());  // same names + layout
```

---

### `get_arch_prefix()`

```cpp
// Header: <boost/typelayout/signature.hpp>
[[nodiscard]] consteval auto get_arch_prefix() noexcept;
```

Returns the platform prefix string: `"[64-le]"`, `"[64-be]"`, `"[32-le]"`,
or `"[32-be]"` based on `sizeof(void*)` and endianness.

This prefix is automatically prepended by `get_layout_signature` and
`get_definition_signature`. You rarely need to call this directly.

---

### `is_fixed_enum<T>()`

```cpp
// Header: <boost/typelayout/detail/reflect.hpp>
template <typename T>
[[nodiscard]] consteval bool is_fixed_enum() noexcept;
```

Returns `true` if enum type `T` has a fixed underlying type.

**Preconditions**: `T` must be an enum type (`static_assert` enforced).

**Known limitation**: For unscoped enums without an explicit underlying type,
this function returns `true` (best-effort; the compiler infers a type that
appears fixed). This is a documented conservative behavior.

---

## Opaque Type Macros

All macros must be invoked inside `namespace boost { namespace typelayout { } }`.

**Header**: `<boost/typelayout/opaque.hpp>`

### `TYPELAYOUT_OPAQUE_TYPE(Type, name, size, align)`

Register a non-template type with a fixed opaque signature.

| Parameter | Description |
|-----------|-------------|
| `Type` | Fully qualified type name |
| `name` | Short display name (string literal) |
| `size` | `sizeof(Type)` -- verified by `static_assert` |
| `align` | `alignof(Type)` -- verified by `static_assert` |

**Generated signature**: `name[s:SIZE,a:ALIGN]`

**Example**:
```cpp
namespace boost { namespace typelayout {
TYPELAYOUT_OPAQUE_TYPE(MyLib::XString, "xstring", 32, 8)
}} // produces: xstring[s:32,a:8]
```

---

### `TYPELAYOUT_OPAQUE_CONTAINER(Template, name, size, align)`

Register a single-type-parameter template with an opaque signature that
includes the element type's signature.

| Parameter | Description |
|-----------|-------------|
| `Template` | Template name without `<T>` |
| `name` | Short display name |
| `size` | `sizeof(Template<T>)` -- must be constant for all `T` |
| `align` | `alignof(Template<T>)` |

**Generated signature**: `name[s:SIZE,a:ALIGN]<element_signature>`

**Example**:
```cpp
TYPELAYOUT_OPAQUE_CONTAINER(MyLib::XVector, "xvector", 24, 1)
// XVector<int32_t> -> xvector[s:24,a:1]<i32[s:4,a:4]>
```

---

### `TYPELAYOUT_OPAQUE_MAP(Template, name, size, align)`

Register a two-type-parameter template with an opaque signature that
includes both key and value type signatures.

| Parameter | Description |
|-----------|-------------|
| `Template` | Template name without `<K, V>` |
| `name` | Short display name |
| `size` | `sizeof(Template<K, V>)` -- must be constant for all `K`, `V` |
| `align` | `alignof(Template<K, V>)` |

**Generated signature**: `name[s:SIZE,a:ALIGN]<key_signature,value_signature>`

**Example**:
```cpp
TYPELAYOUT_OPAQUE_MAP(MyLib::XMap, "xmap", 48, 1)
// XMap<int32_t, double> -> xmap[s:48,a:1]<i32[s:4,a:4],f64[s:8,a:8]>
```

---

## Cross-Platform Tools

### `SigExporter`

```cpp
// Header: <boost/typelayout/tools/sig_export.hpp>
class SigExporter {
public:
    SigExporter();                                    // auto-detect platform
    explicit SigExporter(const std::string& platform_name);

    template <typename T>
    void add(const std::string& type_name);           // register a type

    void write(const std::string& output_dir);        // write .sig.hpp file
};
```

Phase 1 tool: exports type signatures to `.sig.hpp` header files.

**Example**:
```cpp
SigExporter ex;
ex.add<PacketHeader>("PacketHeader");
ex.write("sigs/");
// creates sigs/x86_64_linux_clang.sig.hpp
```

---

### `TYPELAYOUT_EXPORT_TYPES(...)`

```cpp
// Header: <boost/typelayout/tools/sig_export.hpp>
#define TYPELAYOUT_EXPORT_TYPES(...)
```

Convenience macro that generates `main()` and exports all listed types.
Accepts `argv[1]` as the output directory (defaults to current directory).

**Example**:
```cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "my_types.hpp"

TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord)
```

---

### `CompatReporter`

```cpp
// Header: <boost/typelayout/tools/compat_check.hpp>
class CompatReporter {
public:
    void add_platform(/* platform info + signatures */);
    void report(std::ostream& os);
};
```

Phase 2 tool: compares `.sig.hpp` files and produces a compatibility report.
Typically used via the convenience macros below.

---

### `TYPELAYOUT_CHECK_COMPAT(...)`

```cpp
// Header: <boost/typelayout/tools/compat_auto.hpp>
#define TYPELAYOUT_CHECK_COMPAT(...)
```

Generates `main()` that produces a runtime compatibility report for the
listed platforms. Platforms are identified by their namespace names from
the `.sig.hpp` files.

**Example**:
```cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>

TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang)
```

---

### `TYPELAYOUT_ASSERT_COMPAT(first, ...)`

```cpp
// Header: <boost/typelayout/tools/compat_auto.hpp>
#define TYPELAYOUT_ASSERT_COMPAT(first, ...)
```

Compile-time variant of `TYPELAYOUT_CHECK_COMPAT`. Uses `static_assert` to
fail compilation if any type's layout differs across the listed platforms.

---

## Supporting Types

### `FixedString<N>`

```cpp
// Header: <boost/typelayout/fixed_string.hpp>
template <size_t N>
struct FixedString {
    char value[N + 1];
    static constexpr size_t size = N;

    constexpr FixedString();
    constexpr FixedString(const char (&str)[N + 1]);
    constexpr FixedString(std::string_view sv);

    constexpr size_t length() const noexcept;
    constexpr const char* c_str() const noexcept;
    constexpr operator std::string_view() const noexcept;

    template <size_t M>
    constexpr auto operator+(const FixedString<M>& other) const noexcept;

    template <size_t M>
    constexpr bool operator==(const FixedString<M>& other) const noexcept;
    constexpr bool operator==(const char* other) const noexcept;

    consteval auto skip_first() const noexcept;  // strip leading character
};
```

Compile-time fixed-size string. `N` is the character count (excluding null
terminator). All signature functions return `FixedString` instances.

CTAD: `FixedString("hello")` deduces `FixedString<5>`.

---

### `SignatureMode`

```cpp
// Header: <boost/typelayout/fwd.hpp>
enum class SignatureMode { Layout, Definition };
```

Selects which signature layer to compute. Used internally by `TypeSignature`
specializations and by the opaque macros.

---

### `to_fixed_string(num)`

```cpp
// Header: <boost/typelayout/fixed_string.hpp>
template <typename T>
constexpr FixedString<20> to_fixed_string(T num) noexcept;
```

Converts an integer to a `FixedString<20>` (sufficient for `uint64_t` max
value of 20 digits). Used internally for embedding numeric values in
signatures.

---

## Header Map

| Header | Contents |
|--------|----------|
| `typelayout.hpp` | Umbrella -- includes everything below |
| `signature.hpp` | `get_layout_signature`, `get_definition_signature`, match functions |
| `fixed_string.hpp` | `FixedString<N>`, `to_fixed_string` |
| `fwd.hpp` | `SignatureMode`, forward declarations |
| `opaque.hpp` | `TYPELAYOUT_OPAQUE_*` macros |
| `tools/sig_export.hpp` | `SigExporter`, `TYPELAYOUT_EXPORT_TYPES` |
| `tools/compat_check.hpp` | `CompatReporter` |
| `tools/compat_auto.hpp` | `TYPELAYOUT_CHECK_COMPAT`, `TYPELAYOUT_ASSERT_COMPAT` |
