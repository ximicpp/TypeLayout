# TypeLayout API Reference

Complete reference for all public symbols in Boost.TypeLayout.

**Umbrella header**: `<boost/typelayout/typelayout.hpp>`

**Namespace**: `boost::typelayout` unless otherwise noted.

## Public API ("Big 6")

TypeLayout exposes 6 public concepts, but most users only need to start with 3 core
questions:

| Question | Primary API | Purpose |
|----------|-------------|---------|
| What is the byte layout of `T`? | `get_layout_signature<T>()` | Compile-time deterministic layout signature |
| Is `T` safe for byte-copy transport? | `is_byte_copy_safe_v<T>` | Compile-time admission gate for transport safety |
| Can local `T` be transferred to a remote endpoint? | `is_transfer_safe<T>(sig)` | Runtime cross-endpoint verification |

The remaining public concepts are extension mechanisms built around those 3 questions:

| Extension | Header | Purpose |
|-----------|--------|---------|
| Opaque registration macros | `opaque.hpp` | Declare third-party / unanalyzable types |
| `SigExporter` | `tools/sig_export.hpp` | Phase 1: export signatures per platform |
| `CompatReporter` | `tools/compat_check.hpp` | Phase 2: cross-platform compatibility report |

Internal utilities (in `detail::`, not intended as stable user-facing concepts):
- `detail::layout_traits<T>` -- internal layout inspection aggregate used by core/tooling
- `detail::SafetyLevel` / `detail::classify_signature()` -- report-oriented safety classification
- `FixedString<N>` -- compile-time string return type of `get_layout_signature`

---

## Core: Signature Generation

### `get_layout_signature<T>()`

```cpp
// Header: <boost/typelayout/signature.hpp>
template <typename T>
[[nodiscard]] consteval auto get_layout_signature() noexcept;
```

Returns a `FixedString` containing the layout signature of `T`.

The signature encodes field types, sizes, alignments, and byte offsets. Field
names and inheritance structure are erased; only byte identity is preserved.
The arch prefix (`[64-le]`, etc.) is prepended automatically.

**Unsupported types** (compile error):
- `void`
- Unbounded arrays (`T[]`)
- Bare function types (`void(int)`)

**Example**:

```cpp
struct Msg { uint32_t id; double value; };
constexpr auto sig = get_layout_signature<Msg>();
// "[64-le]record[s:16,a:8]{@0:u32[s:4,a:4],@8:f64[s:8,a:8]}"
```

**Signature comparison**: Use `get_layout_signature<A>() == get_layout_signature<B>()`
directly. `FixedString` supports `operator==`.

---

## Core: Admission

### `is_byte_copy_safe_v<T>`

```cpp
// Header: <boost/typelayout/admission.hpp>
template <typename T>
inline constexpr bool is_byte_copy_safe_v;
```

Compile-time predicate: `true` if `T` is safe for byte-level transport (memcpy to
a buffer, send over network, write to shared memory). Recursively checks all
members and base classes:

- Trivially copyable types without pointers: safe.
- Registered relocatable opaque types with safe elements: safe.
- Types with pointers, references, or unregistered opaque members: unsafe.

**Example**:

```cpp
struct Vec3 { float x, y, z; };
static_assert(is_byte_copy_safe_v<Vec3>);    // true: all floats

struct Node { int val; Node* next; };
static_assert(!is_byte_copy_safe_v<Node>);   // false: has pointer
```

---

### FixedString\<N\>

```cpp
// Header: <boost/typelayout/fixed_string.hpp>
template <std::size_t N>
struct FixedString {
    char value[N + 1];
    static constexpr std::size_t size = N;  // character count, excluding null terminator

    constexpr FixedString() noexcept;
    constexpr FixedString(const char (&str)[N + 1]) noexcept;

    constexpr std::size_t length() const noexcept;   // == N
    constexpr const char* c_str() const noexcept;
    constexpr operator std::string_view() const noexcept;

    template <std::size_t M>
    constexpr FixedString<N + M> operator+(const FixedString<M>&) const noexcept;

    template <std::size_t M>
    constexpr bool operator==(const FixedString<M>&) const noexcept;
    constexpr bool operator==(const char*) const noexcept;

    constexpr bool contains(std::string_view needle) const noexcept;
    constexpr bool contains_token(std::string_view needle) const noexcept;
    constexpr std::size_t find(std::string_view needle) const noexcept;
    consteval auto skip_first() const noexcept;  // strip leading character
};
```

Compile-time fixed-size string. `N` is the exact character count (no wasted
capacity). All signature functions return `FixedString` instances.

CTAD: `FixedString{"hello"}` deduces `FixedString<5>`.

### `to_fixed_string<V>()`

```cpp
// Header: <boost/typelayout/fixed_string.hpp>
template <auto V>
consteval auto to_fixed_string() noexcept;  // returns FixedString<digits(V)>

template <typename T>
constexpr FixedString<20> to_fixed_string(T num) noexcept;  // runtime/constexpr, generic
```

Converts an integer to a `FixedString`. The `consteval` form deduces the exact
number of digits at compile time. The `constexpr` form returns `FixedString<20>`,
which is sufficient for the maximum value of `uint64_t` (20 decimal digits).

---

## Core: Opaque Registration

**Header**: `<boost/typelayout/opaque.hpp>`

All macros must be invoked inside `namespace boost { namespace typelayout { } }`.

Opaque registration is an extension mechanism. Use it only when reflection alone is
not enough to describe the type you want to transport.

### 1. Ordinary opaque types

### `TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)` (recommended)

```cpp
TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)
```

Registers `Type` as an opaque type with signature fragment `O(Tag|sizeof|alignof)`.

| Parameter | Description |
|-----------|-------------|
| `Type` | Fully qualified C++ type |
| `Tag` | String tag embedded in the signature |
| `HasPointer` | `true` if the type internally contains pointer-like fields |

Requires `std::is_trivially_copyable_v<Type>`.

**Example**:

```cpp
namespace boost { namespace typelayout {
TYPELAYOUT_REGISTER_OPAQUE(MyLib::Handle, "handle", false)
// MyLib::Handle -> O(handle|8|8)
}}
```

For template types, register each instantiation separately:

```cpp
namespace boost { namespace typelayout {
TYPELAYOUT_REGISTER_OPAQUE(MyLib::XVector<int>, "xvector_int", true)
TYPELAYOUT_REGISTER_OPAQUE(MyLib::XMap<int, double>, "xmap_int_double", true)
}}
```

### 2. Relocatable opaque types

Use the relocatable opaque macros when the type is not trivially copyable but you are
explicitly asserting that byte relocation is still valid for the intended transport
workflow.

This group covers:
- non-trivially-copyable concrete wrappers
- single-parameter container-like opaque templates
- key/value map-like opaque templates

These macros belong to the extension layer, not the core 3-question mental model.

---

## Tools

### SigExporter

**Header**: `<boost/typelayout/tools/sig_export.hpp>` (requires P2996)

### `SigExporter`

```cpp
class SigExporter {
public:
    SigExporter();                                         // auto-detect platform
    explicit SigExporter(const std::string& platform_name); // explicit platform name

    template <typename T>
    void add(const std::string& name);                     // requires trivially_copyable

    int write(const std::string& path) const;              // write .sig.hpp; returns 0 on success
    const std::string& platform_name() const;
};
```

Phase 1 tool: collects type signatures and writes a `.sig.hpp` header file that
can be included by Phase 2 programs for comparison.

`add<T>` requires `std::is_trivially_copyable_v<T>` (enforced by `static_assert`).

**Example**:

```cpp
SigExporter ex;
ex.add<PacketHeader>("PacketHeader");
ex.add<SensorRecord>("SensorRecord");
int rc = ex.write("sigs/x86_64_linux_clang.sig.hpp");  // 0 on success
```

### `TYPELAYOUT_EXPORT_TYPES(...)`

```cpp
// Header: <boost/typelayout/tools/sig_export.hpp>
TYPELAYOUT_EXPORT_TYPES(Type1, Type2, ...)
```

Generates `main()` that creates a `SigExporter`, registers all listed types, and
writes the output. `argv[1]` is the output directory; if omitted, writes to stdout.

```cpp
#include <boost/typelayout/tools/sig_export.hpp>
#include "my_types.hpp"

TYPELAYOUT_EXPORT_TYPES(PacketHeader, SensorRecord, SharedMemRegion)
// compile with P2996, run: ./sig_export sigs/
```

### `TYPELAYOUT_REGISTER_TYPES(exporter, ...)`

```cpp
TYPELAYOUT_REGISTER_TYPES(exporter, Type1, Type2, ...)
```

Registers types on an existing `SigExporter` without generating `main()`.
Use when you need custom logic around the exporter.

---

### CompatReporter

**Header**: `<boost/typelayout/tools/compat_check.hpp>`

### `CompatReporter`

```cpp
namespace boost::typelayout::compat {

class CompatReporter {
public:
    void add_platform(const std::string& name,
                      const TypeEntry* types, std::size_t count);
    void add_platform(const PlatformData& pd);

    std::vector<TypeResult> compare() const;
    void print_report(std::ostream& os = std::cout) const;
    void print_diff_report(std::ostream& os = std::cout) const;  // with ^--- diff annotations
};

} // namespace boost::typelayout::compat
```

Phase 2 tool: compares `.sig.hpp` files across platforms and produces a
compatibility report. Typically used via the macros below.

### `TYPELAYOUT_CHECK_COMPAT(...)`

```cpp
// Header: <boost/typelayout/tools/compat_auto.hpp>
TYPELAYOUT_CHECK_COMPAT(platform1, platform2, ...)
```

Generates `main()` that produces a runtime compatibility report and exits 0.
Platform names must match the namespace names from the included `.sig.hpp` headers.

```cpp
#include "sigs/x86_64_linux_clang.sig.hpp"
#include "sigs/arm64_macos_clang.sig.hpp"
#include <boost/typelayout/tools/compat_auto.hpp>

TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang, arm64_macos_clang)
```

### `TYPELAYOUT_ASSERT_COMPAT(...)`

```cpp
// Header: <boost/typelayout/tools/compat_auto.hpp>
TYPELAYOUT_ASSERT_COMPAT(platform1, platform2, ...)
```

Compile-time variant. Uses `static_assert` for each type comparison; compilation
fails if any type's layout differs across the listed platforms.

---

## Signature Format

```
full-signature ::= arch-prefix type-signature
arch-prefix    ::= '[' ('32'|'64') '-' ('le'|'be') ']'
type-signature ::= leaf | record | union-sig | enum-sig | array | opaque

leaf      ::= kind '[s:' SIZE ',a:' ALIGN ']'
kind      ::= 'u8'|'i8'|'u16'|'i16'|'u32'|'i32'|'u64'|'i64'
            | 'f32'|'f64'|'fld'|'bool'|'char'|'uchar'
            | 'char8'|'char16'|'char32'|'wchar'
            | 'ptr'|'fnptr'|'memptr'|'ref'|'rref'
            | 'nullptr'|'void_ptr'
record    ::= 'record[s:' SIZE ',a:' ALIGN ']{' member-list '}'
union-sig ::= 'union[s:' SIZE ',a:' ALIGN ']{' member-list '}'
enum-sig  ::= 'enum[s:' SIZE ',a:' ALIGN ']<' underlying '>'
array     ::= 'array[s:' SIZE ',a:' ALIGN ']<' element ',' COUNT '>'
            | 'bytes[s:' SIZE ',a:1]'           -- byte arrays
opaque    ::= 'O(' tag '|' SIZE '|' ALIGN ')'  -- TYPELAYOUT_REGISTER_OPAQUE

member    ::= '@' OFFSET ':' type-signature
            | '@' BYTE '.' BIT ':bits<' WIDTH ',' leaf '>'  -- bit-field
```

Inheritance is flattened. Padding is not encoded; it is implied by offset gaps
and detected by comparing byte coverage against `sizeof(T)`.

---

## Header Map

| Header | Layer | Contents |
|--------|-------|----------|
| `<boost/typelayout/typelayout.hpp>` | Umbrella | Includes all Core headers |
| `<boost/typelayout/signature.hpp>` | Core | `get_layout_signature<T>()` |
| `<boost/typelayout/admission.hpp>` | Core | `is_byte_copy_safe_v<T>` |
| `<boost/typelayout/opaque.hpp>` | Core | `TYPELAYOUT_REGISTER_OPAQUE` macros |
| `<boost/typelayout/tools/transfer.hpp>` | Core | `is_transfer_safe<T>(sig)` |
| `<boost/typelayout/fixed_string.hpp>` | Internal | `FixedString<N>`, `to_fixed_string` |
| `<boost/typelayout/layout_traits.hpp>` | Internal | `detail::layout_traits<T>` |
| `<boost/typelayout/tools/safety_level.hpp>` | Internal | `detail::SafetyLevel`, `detail::classify_signature` |
| `<boost/typelayout/tools/sig_export.hpp>` | Tools | `SigExporter`, `TYPELAYOUT_EXPORT_TYPES` |
| `<boost/typelayout/tools/compat_check.hpp>` | Tools | `compat::CompatReporter` |
| `<boost/typelayout/tools/compat_auto.hpp>` | Tools | `TYPELAYOUT_CHECK_COMPAT`, `TYPELAYOUT_ASSERT_COMPAT` |
| `<boost/typelayout/tools/platform_detect.hpp>` | Internal | Arch/OS/compiler detection macros |
