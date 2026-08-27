#include "evidence_json.hpp"
#include "world.hpp"

#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

#ifndef TYPELAYOUT_TOOLCHAIN_REVISION
#error "TYPELAYOUT_TOOLCHAIN_REVISION must be injected by CMake"
#endif

#ifndef TYPELAYOUT_COMPILER_TARGET
#error "TYPELAYOUT_COMPILER_TARGET must be injected by CMake"
#endif

#define TYPELAYOUT_STRINGIZE_DETAIL(value) #value
#define TYPELAYOUT_STRINGIZE(value) TYPELAYOUT_STRINGIZE_DETAIL(value)

namespace {

using relocatable_world_demo::evidence_json::write_key;
using relocatable_world_demo::evidence_json::write_string;

struct arguments {
    std::string_view node;
    std::string_view output;
    std::string_view runner;
    std::string_view runner_image;
    std::string_view xcode_version;
    std::string_view xcode_build;
    std::string_view sdk_version;
    std::string_view sdk_build;
    std::string_view deployment_target;
    bool sdk_locked;
};

bool parse_boolean(std::string_view text, bool& value) {
    if (text == "true") {
        value = true;
        return true;
    }
    if (text == "false") {
        value = false;
        return true;
    }
    return false;
}

bool set_once(std::string_view& destination, std::string_view value) {
    if (!destination.empty() || value.empty()) {
        return false;
    }
    destination = value;
    return true;
}

bool parse_arguments(int argc, char** argv, arguments& result) {
    if (argc != 19) {
        return false;
    }
    result.node = argv[1];
    result.output = argv[2];
    std::string_view sdk_locked_text;
    for (int index = 3; index < argc; index += 2) {
        const std::string_view option{argv[index]};
        const std::string_view value{argv[index + 1]};
        if (option == "--runner") {
            if (!set_once(result.runner, value)) {
                return false;
            }
        } else if (option == "--runner-image") {
            if (!set_once(result.runner_image, value)) {
                return false;
            }
        } else if (option == "--xcode-version") {
            if (!set_once(result.xcode_version, value)) {
                return false;
            }
        } else if (option == "--xcode-build") {
            if (!set_once(result.xcode_build, value)) {
                return false;
            }
        } else if (option == "--sdk-version") {
            if (!set_once(result.sdk_version, value)) {
                return false;
            }
        } else if (option == "--sdk-build") {
            if (!set_once(result.sdk_build, value)) {
                return false;
            }
        } else if (option == "--deployment-target") {
            if (!set_once(result.deployment_target, value)) {
                return false;
            }
        } else if (option == "--sdk-locked") {
            if (!set_once(sdk_locked_text, value)) {
                return false;
            }
        } else {
            return false;
        }
    }
    if (result.node.empty() || result.output.empty() || result.runner.empty() ||
        result.runner_image.empty() || result.xcode_version.empty() ||
        result.xcode_build.empty() || result.sdk_version.empty() ||
        result.sdk_build.empty() || result.deployment_target.empty() ||
        sdk_locked_text.empty()) {
        return false;
    }
    return parse_boolean(sdk_locked_text, result.sdk_locked);
}

constexpr std::string_view compiler_family() {
#if defined(__clang__)
    return "clang";
#elif defined(__GNUC__)
    return "gcc";
#else
    return "unsupported";
#endif
}

constexpr std::string_view compiler_version() {
#if defined(__clang__)
    return __clang_version__;
#elif defined(__GNUC__)
    return __VERSION__;
#else
    return "unsupported";
#endif
}

constexpr std::string_view standard_library() {
#if defined(_LIBCPP_VERSION)
    return "libc++-" TYPELAYOUT_STRINGIZE(_LIBCPP_VERSION);
#elif defined(__GLIBCXX__)
    return "libstdc++-" TYPELAYOUT_STRINGIZE(__GLIBCXX__);
#else
    return "unsupported";
#endif
}

constexpr std::string_view current_node() {
#if defined(__x86_64__) || defined(_M_X64)
    constexpr std::string_view architecture = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    constexpr std::string_view architecture = "arm64";
#else
    constexpr std::string_view architecture = "unsupported";
#endif

#if defined(__APPLE__)
    constexpr std::string_view operating_system = "macos";
#elif defined(__linux__)
    constexpr std::string_view operating_system = "linux";
#else
    constexpr std::string_view operating_system = "unsupported";
#endif

    if (architecture == "x86_64" && operating_system == "linux" &&
        compiler_family() == "gcc") {
        return "x86_64_linux_gcc";
    }
    if (architecture == "x86_64" && operating_system == "linux" &&
        compiler_family() == "clang") {
        return "x86_64_linux_clang";
    }
    if (architecture == "arm64" && operating_system == "linux" &&
        compiler_family() == "gcc") {
        return "arm64_linux_gcc";
    }
    if (architecture == "arm64" && operating_system == "linux" &&
        compiler_family() == "clang") {
        return "arm64_linux_clang";
    }
    if (architecture == "arm64" && operating_system == "macos" &&
        compiler_family() == "clang") {
        return "arm64_macos_clang";
    }
    if (architecture == "x86_64" && operating_system == "macos" &&
        compiler_family() == "clang") {
        return "x86_64_macos_clang";
    }
    return "unsupported";
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
void* copy_distinct(void* destination, const void* source,
                    std::size_t byte_count) {
    if (destination == source || byte_count == 0) {
        return nullptr;
    }
    return std::memcpy(destination, source, byte_count);
}

struct lifetime_results {
    bool object;
    bool array;
};

#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
std::size_t runtime_array_count(const char* node) {
    std::size_t count = 0;
    while (count != 3 && node[count] != '\0') {
        ++count;
    }
    return count;
}

lifetime_results probe_memcpy_lifetime(std::size_t array_count) {
    std::uint32_t one_source = 7;
    alignas(std::uint32_t) std::byte one_storage[sizeof(one_source)]{};
    auto* one = static_cast<std::uint32_t*>(
        copy_distinct(one_storage, &one_source, sizeof(one_source)));
    if (one == nullptr) {
        return {false, false};
    }
    *one += 5;

    std::array<std::uint32_t, 3> array_source{11, 13, 17};
    alignas(std::uint32_t) std::byte array_storage[sizeof(array_source)]{};
    auto* values = static_cast<std::uint32_t*>(copy_distinct(
        array_storage, array_source.data(), array_count * sizeof(std::uint32_t)));
    if (values == nullptr || array_count != array_source.size()) {
        return {false, false};
    }
    std::span<std::uint32_t> view(values, array_count);
    view[1] += *one;

    alignas(std::uint32_t)
        std::byte relocated_storage[sizeof(array_source)]{};
    auto* relocated = static_cast<std::uint32_t*>(copy_distinct(
        relocated_storage, array_storage,
        array_count * sizeof(std::uint32_t)));
    if (relocated == nullptr) {
        return {false, false};
    }
    view[0] = 19;

    const bool object_ok = one_source == 7 && *one == 12;
    const bool array_ok = relocated[0] == 11 && relocated[1] == 25 &&
        relocated[2] == 17 && relocated + array_count - relocated == 3 &&
        view[0] == 19;
    return {object_ok, array_ok};
}

bool json_emitter_self_test() {
    std::ostringstream output;
    write_string(output, std::string_view{"\"\\\n\x01", 4});
    return output.str() == "\"\\\"\\\\\\u000a\\u0001\"";
}

void write_boolean(std::ostream& output, bool value) {
    output << (value ? "true" : "false");
}

bool write_probe_json(const arguments& args, lifetime_results lifetime) {
    std::ofstream output(std::string(args.output),
                         std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cerr << "cannot open probe output: " << args.output << '\n';
        return false;
    }

    constexpr auto reflected_int =
        boost::typelayout::get_layout_signature<int>();

    output << "{\n  ";
    write_key(output, "schema");
    output << "1,\n  ";
    write_key(output, "node");
    write_string(output, args.node);
    output << ",\n  ";
    write_key(output, "probe");
    output << "{\n    ";
    write_key(output, "char_bit");
    output << CHAR_BIT << ",\n    ";
    write_key(output, "pointer_bits");
    output << sizeof(void*) * CHAR_BIT << ",\n    ";
    write_key(output, "endian");
    write_string(output,
        std::endian::native == std::endian::little ? "little" : "big");
    output << ",\n    ";
    write_key(output, "reflection");
    write_boolean(output, reflected_int.length() != 0);
    output << ",\n    ";
    write_key(output, "memcpy_object_lifetime");
    write_boolean(output, lifetime.object);
    output << ",\n    ";
    write_key(output, "memcpy_array_lifetime");
    write_boolean(output, lifetime.array);
    output << "\n  },\n  ";

    write_key(output, "admission");
    output << "{\n";
    std::size_t admission_index = 0;
    relocatable_world_demo::for_each_contract_type(
        [&]<typename T>(std::string_view key) {
            output << "    ";
            write_key(output, key);
            write_boolean(output, boost::typelayout::is_admitted_v<
                T, relocatable_world_demo::whole_region_profile>);
            ++admission_index;
            output << (admission_index == 4 ? "\n" : ",\n");
        });
    output << "  },\n  ";

    write_key(output, "compiler");
    output << "{\n    ";
    write_key(output, "family");
    write_string(output, compiler_family());
    output << ",\n    ";
    write_key(output, "revision");
    write_string(output, TYPELAYOUT_TOOLCHAIN_REVISION);
    output << ",\n    ";
    write_key(output, "version");
    write_string(output, compiler_version());
    output << ",\n    ";
    write_key(output, "target");
    write_string(output, TYPELAYOUT_COMPILER_TARGET);
    output << ",\n    ";
    write_key(output, "stdlib");
    write_string(output, standard_library());
    output << ",\n    ";
    write_key(output, "xcode_version");
    write_string(output, args.xcode_version);
    output << ",\n    ";
    write_key(output, "xcode_build");
    write_string(output, args.xcode_build);
    output << ",\n    ";
    write_key(output, "sdk_version");
    write_string(output, args.sdk_version);
    output << ",\n    ";
    write_key(output, "sdk_build");
    write_string(output, args.sdk_build);
    output << ",\n    ";
    write_key(output, "deployment_target");
    write_string(output, args.deployment_target);
    output << ",\n    ";
    write_key(output, "sdk_locked");
    write_boolean(output, args.sdk_locked);
    output << "\n  },\n  ";

    write_key(output, "environment");
    output << "{\n    ";
    write_key(output, "runner");
    write_string(output, args.runner);
    output << ",\n    ";
    write_key(output, "runner_image");
    write_string(output, args.runner_image);
    output << "\n  }\n}\n";
    output.close();
    return static_cast<bool>(output);
}

} // namespace

static_assert(CHAR_BIT == 8);
static_assert(sizeof(void*) == 8);
static_assert(std::endian::native == std::endian::little);
constexpr auto reflected_int = boost::typelayout::get_layout_signature<int>();
static_assert(reflected_int.length() != 0);

int main(int argc, char** argv) {
    arguments args{};
    if (!parse_arguments(argc, argv, args)) {
        std::cerr << "usage: relocatable_world_platform_probe NODE OUTPUT_JSON "
                     "--runner LABEL --runner-image ID "
                     "--xcode-version VALUE --xcode-build VALUE "
                     "--sdk-version VALUE --sdk-build VALUE "
                     "--deployment-target VALUE "
                     "--sdk-locked true|false\n";
        return 2;
    }
    if (args.node != current_node()) {
        std::cerr << "node does not match compiler target: requested="
                  << args.node << " actual=" << current_node() << '\n';
        return 2;
    }
    if (!json_emitter_self_test()) {
        std::cerr << "JSON emitter escaping self-test failed\n";
        return 1;
    }

    const auto lifetime = probe_memcpy_lifetime(
        runtime_array_count(argv[1]));
    if (!write_probe_json(args, lifetime)) {
        return 1;
    }
    if (!lifetime.object || !lifetime.array) {
        std::cerr << "TOOLCHAIN PROBE FAIL node=" << args.node
                  << " object=" << lifetime.object
                  << " array=" << lifetime.array << '\n';
        return 1;
    }
    std::cout << "TOOLCHAIN PROBE PASS node=" << args.node << '\n';
    return 0;
}

#undef TYPELAYOUT_STRINGIZE
#undef TYPELAYOUT_STRINGIZE_DETAIL
