Generate and display the layout signature and traits for a given C++ type.

The user provides a type (e.g., `$ARGUMENTS`) and this skill compiles a temporary program that prints the signature and layout_traits results.

## Steps

1. Take the type name from `$ARGUMENTS`. If empty, ask the user for a type.
2. Create a temporary source file `/tmp/sig_check.cpp` that:
   - Includes the necessary TypeLayout headers
   - Includes any headers the user specifies (ask if the type is not a builtin/standard type)
   - Uses `get_layout_signature<T>()` and `detail::layout_traits<T>` to print:
     - The full signature string
     - `has_pointer`
     - `is_byte_copy_safe`
     - Safety classification via `classify_signature()`
3. Compile and run using the appropriate method (WSL, local compiler, or Docker)

## Compiler Selection

Prefer GCC 16+ over Bloomberg Clang. The CMakeLists.txt auto-detects flags, but for
one-off compilation:

**GCC 16**:
```bash
g++ -std=c++26 -freflection -I include -o /tmp/sig_check /tmp/sig_check.cpp
```

**Bloomberg Clang (WSL/legacy)**:
```bash
CXX=/root/clang-p2996-install/bin/clang++
$CXX -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
  -I include -o /tmp/sig_check /tmp/sig_check.cpp
```

**Docker (GCC 16)**:
```bash
docker run --rm --platform linux/amd64 \
  -v $(pwd):/workspace -w /workspace \
  sourcemation/gcc-16 \
  bash -c 'apt-get update -qq && apt-get install -y -qq cmake > /dev/null 2>&1 && \
  g++ -std=c++26 -freflection -I include -o /tmp/sig_check /tmp/sig_check.cpp && \
  /tmp/sig_check'
```

## Test Source Template

```cpp
#include <boost/typelayout/typelayout.hpp>
#include <boost/typelayout/tools/safety_level.hpp>
#include <iostream>
#include <cstdint>

// USER_HEADERS_HERE

using namespace boost::typelayout;

// USER_TYPE_DEFINITION_HERE

int main() {
    using T = USER_TYPE;
    constexpr auto sig = get_layout_signature<T>();
    constexpr bool has_ptr = detail::layout_traits<T>::has_pointer;
    constexpr bool copy_safe = is_byte_copy_safe_v<T>;
    auto level = compat::detail::classify_signature(
        std::string_view(sig.value, sig.size()));

    std::cout << "Type:            USER_TYPE\n";
    std::cout << "Signature:       " << sig.value << "\n";
    std::cout << "Size:            " << sizeof(T) << "\n";
    std::cout << "Alignment:       " << alignof(T) << "\n";
    std::cout << "has_pointer:     " << has_ptr << "\n";
    std::cout << "byte_copy_safe:  " << copy_safe << "\n";
    std::cout << "Safety:          "
              << compat::detail::safety_level_name(level) << "\n";
    return 0;
}
```

## Important Notes

- Replace `USER_TYPE` with the actual type
- For user-defined structs, define them inline in the source or include the relevant header
- For types requiring opaque registration, include `opaque.hpp` and the registration macros
- The include path `-I include` points to the project's header directory
- Clean up: the temporary files are in `/tmp/` and will be cleaned automatically
