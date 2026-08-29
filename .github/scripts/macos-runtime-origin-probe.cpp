#if !defined(__APPLE__)
#error "This runtime origin probe is macOS-only"
#endif

#include <cxxabi.h>
#include <dlfcn.h>
#include <unwind.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

[[noreturn]] void fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

template <class Pointer>
const void* function_address(Pointer pointer) {
    static_assert(sizeof(pointer) == sizeof(const void*));
    const void* address = nullptr;
    std::memcpy(&address, &pointer, sizeof(address));
    return address;
}

void print_origin(const char* label, const void* address) {
    Dl_info information{};
    if (dladdr(address, &information) == 0 || information.dli_fname == nullptr) {
        fail("dladdr could not resolve a runtime symbol");
    }
    char resolved[PATH_MAX]{};
    if (realpath(information.dli_fname, resolved) == nullptr) {
        fail("realpath could not resolve a runtime image");
    }
    std::printf("%s\t%s\n", label, resolved);
}

} // namespace

int main() {
    print_origin("libc++", &std::cout);
    print_origin(
        "libc++abi",
        function_address(&__cxxabiv1::__cxa_demangle));
    print_origin("libunwind", function_address(&_Unwind_GetIP));
}
