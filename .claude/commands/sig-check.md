Generate and display the layout signature and traits for a given C++ type.

The user provides a type (e.g., `$ARGUMENTS`) and this skill compiles a temporary program that prints the signature and layout_traits results.

## Steps

1. Take the type name from `$ARGUMENTS`. If empty, ask the user for a type.
2. Create a temporary source file in WSL `/tmp/sig_check.cpp` that:
   - Includes the necessary TypeLayout headers
   - Includes any headers the user specifies (ask if the type is not a builtin/standard type)
   - Uses `get_layout_signature<T>()` and `layout_traits<T>` to print:
     - The full signature string
     - `has_pointer`
     - `has_opaque`
     - `has_padding`
     - `field_count`
     - Safety classification via `classify<T>`
3. Compile and run in WSL (or macOS/Docker as appropriate)

## Windows (WSL) Example

```bash
wsl -e bash -c 'cd /mnt/g/workspace/TypeLayout && \
  export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && \
  CXX=/root/clang-p2996-install/bin/clang++ && \
  cat > /tmp/sig_check.cpp << '\''SRCEOF'\''
#include <boost/typelayout/typelayout.hpp>
#include <boost/typelayout/tools/classify.hpp>
#include <iostream>
#include <cstdint>

// USER_HEADERS_HERE

using namespace boost::typelayout;

// USER_TYPE_DEFINITION_HERE

int main() {
    using T = USER_TYPE;
    constexpr auto sig = get_layout_signature<T>();
    constexpr auto traits = layout_traits<T>{};
    auto safety = tools::classify<T>();

    std::cout << "Type:         USER_TYPE\n";
    std::cout << "Signature:    " << sig.value << "\n";
    std::cout << "Size:         " << sizeof(T) << "\n";
    std::cout << "Alignment:    " << alignof(T) << "\n";
    std::cout << "has_pointer:  " << traits.has_pointer << "\n";
    std::cout << "has_opaque:   " << traits.has_opaque << "\n";
    std::cout << "has_padding:  " << traits.has_padding << "\n";
    std::cout << "field_count:  " << traits.field_count << "\n";
    std::cout << "Safety:       " << static_cast<int>(safety.level) << " (" << safety.reason << ")\n";
    return 0;
}
SRCEOF
  $CXX -std=c++26 -freflection -freflection-latest -stdlib=libc++ \
    -I include -o /tmp/sig_check /tmp/sig_check.cpp && \
  /tmp/sig_check'
```

## Important Notes

- Replace `USER_TYPE` with the actual type
- For user-defined structs, define them inline in the source or include the relevant header
- For types requiring opaque registration, include `opaque.hpp` and the registration macros
- The include path `-I include` points to the project's header directory
- Clean up: the temporary files are in `/tmp/` and will be cleaned automatically
