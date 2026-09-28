#include "observer.hpp"
#include "build_metadata.hpp"
#ifdef TYPELAYOUT_OBSERVER_REGISTRATION_HEADER
#include TYPELAYOUT_OBSERVER_REGISTRATION_HEADER
#else
#include "fixtures.hpp"
#endif
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <stdexcept>
#if defined(__GLIBC__)
#include <gnu/libc-version.h>
#endif

#ifndef TYPELAYOUT_OBSERVER_REGISTRATION_HEADER
namespace observer {
template<> struct opaque_registration<fixtures::Opaque> {
    static constexpr bool value = true;
    static constexpr const char* tag = "fixtures.Opaque";
};
} // namespace observer
#endif

namespace {
std::string architecture() {
#if defined(__x86_64__)
    return "x64";
#elif defined(__aarch64__)
    return "arm64";
#elif defined(__i386__)
    return "x86";
#else
    return "unknown";
#endif
}
std::string operating_system() {
#if defined(__linux__)
    return "linux";
#elif defined(__APPLE__)
    return "macos";
#elif defined(_WIN32)
    return "windows";
#else
    return "unknown";
#endif
}
observer::Json runtime() {
#if defined(__GLIBC__)
    return observer::Json::object({{"name", "glibc"}, {"version", gnu_get_libc_version()}});
#else
    return observer::Json::object({{"name", "native-runtime"}, {"version", "unknown"}});
#endif
}
} // namespace

int main(int argc, char** argv) {
    using observer::Json;
    try {
        std::locale::global(std::locale::classic());
        std::string configuration = TYPELAYOUT_OBSERVER_CONFIGURATION;
        std::string run_id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::string output;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") {
                std::cout << "typelayout-native [--configuration Debug|Release] [--run-id ID] [--output FILE]\n"
                    "Without --output, the snapshot is written to stdout. Configuration must match the binary.\n";
                return 0;
            }
            if ((arg == "--configuration" || arg == "--run-id" || arg == "--output") && i + 1 < argc) {
                const std::string value = argv[++i];
                if (value.empty()) throw std::runtime_error("empty argument: " + arg);
                if (arg == "--configuration") configuration = value;
                else if (arg == "--run-id") run_id = value;
                else output = value;
            } else throw std::runtime_error("unknown option or missing value: " + arg);
        }
        if (configuration != TYPELAYOUT_OBSERVER_CONFIGURATION)
            throw std::runtime_error("requested configuration " + configuration + " does not match actual binary " + TYPELAYOUT_OBSERVER_CONFIGURATION);
        Json build = Json::object({{"languages", Json::array({"cpp"})}, {"buildId", "native:" + configuration + ":" + TYPELAYOUT_OBSERVER_REVISION},
            {"runId", run_id}, {"configuration", configuration}, {"sourceRevision", TYPELAYOUT_OBSERVER_REVISION},
            {"sourceDirty", TYPELAYOUT_OBSERVER_DIRTY != 0}, {"sourceDigest", "unknown"}, {"artifactDigest", "unknown"},
            {"compiler", Json::object({{"name", "Clang P2996"}, {"version", __clang_version__}})},
            {"runtime", runtime()},
            {"target", Json::object({{"os", operating_system()}, {"architecture", architecture()},
                {"abi", "itanium"}, {"pointerBits", static_cast<std::int64_t>(sizeof(void*) * CHAR_BIT)},
                {"bitsPerByte", CHAR_BIT}, {"endian", std::endian::native == std::endian::little ? "little" : "big"}})},
            {"flags", Json::array({TYPELAYOUT_OBSERVER_COMPILE_FLAGS})},
            {"dependencies", Json::object({{"libc++", std::to_string(_LIBCPP_VERSION)},
                {"bitfieldConvention", "P2996 byte offset plus least-significant bit offset; little-endian fixture calibrated"}})}});
        observer::Collector collector;
#ifdef TYPELAYOUT_OBSERVER_REGISTRATION_HEADER
        register_layouts(collector);
#else
        collector.collect<fixtures::Sample>("sample");
        collector.collect<fixtures::Packed>("packed");
        collector.collect<fixtures::Reordered>("reordered");
        collector.collect<fixtures::Nested>("nested");
        collector.collect<fixtures::Array>("array");
        collector.collect<fixtures::Enum>("enum");
        collector.collect<fixtures::Base>("base");
        collector.collect<fixtures::Derived>("derived");
        collector.collect<fixtures::EmptyMember>("empty-member");
        collector.collect<fixtures::EmptyBase>("empty-base");
        collector.collect<fixtures::Overlap>("overlap");
        collector.collect<fixtures::Bits>("bitfields");
        collector.collect<fixtures::Union>("union");
        collector.collect<fixtures::Opaque>("opaque");
        collector.collect<fixtures::OpaqueHost>("opaque-host");
        collector.collect<fixtures::Nontrivial>("nontrivial");
        collector.collect<fixtures::Polymorphic>("polymorphic");
        collector.collect<fixtures::Virtual>("virtual");
        collector.collect<fixtures::Pointer>("pointer");
        collector.collect<fixtures::Boolean>("boolean");
        collector.collect<fixtures::Utf16>("char16");
#endif
        auto snapshot = collector.finish(std::move(build), run_id);
        if (output.empty() || output == "-") {
            snapshot.write(std::cout); std::cout << '\n';
            if (!std::cout) throw std::runtime_error("failed writing snapshot to stdout");
        } else {
            observer::write_new_snapshot(std::filesystem::path(output), snapshot);
            std::cerr << "Captured native snapshot: " << output << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native-capture-error: " << error.what() << '\n';
        return 3;
    }
}
