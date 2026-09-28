#include "fixtures.hpp"
#include "../json.hpp"
#include <bit>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <stdexcept>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using observer::Json;
using namespace marshal_fixtures;

namespace {
Json scalar(const std::string& id, std::size_t width, const std::string& category, const std::string& signedness, const std::string& encoding) {
    return Json::object({{"id", id}, {"displayName", id}, {"kind", "scalar"}, {"representation", Json::object({
        {"widthBits", observer::number(width, "sizeof(registered C++ scalar)*8")},
        {"category", observer::known(category, "explicit fixture C++ field type")},
        {"signedness", observer::known(signedness, "registered scalar/code-unit semantics")},
        {"encoding", observer::known(encoding, "registered fixed-width scalar representation")},
        {"floatingFormat", observer::not_applicable("not-floating-point")}})}});
}
Json field(const std::string& id, const std::string& type, std::size_t offset, std::size_t width, int order) {
    return Json::object({{"id", id}, {"displayName", id}, {"role", "field"}, {"typeRef", type}, {"declarationOrder", order},
        {"offsetBits", observer::number(offset * 8, "offsetof(registered standard-layout fixture, field)*8")},
        {"bitWidth", observer::number(width * 8, "sizeof(registered field)*8")},
        {"declaredTypeSizeBits", observer::number(width * 8, "sizeof(registered field)*8")},
        {"occupiedRanges", observer::range(offset * 8, width * 8, "offsetof+sizeof ordinary fixture field")}});
}
Json observation(const std::string& id, const std::string& type, std::size_t bytes, std::size_t alignment,
    Json::Array fields = {}, Json context = Json::object({{"kind", "complete-value"}})) {
    return Json::object({{"id", id}, {"typeId", type}, {"displayName", type}, {"status", "ok"}, {"view", "native"},
        {"context", std::move(context)}, {"origin", Json::object({{"kind", "value-start"}, {"extentStartBit", 0}})},
        {"metrics", Json::object({{"valueSizeBytes", observer::number(bytes, "sizeof(registered native type)")},
            {"standaloneSizeBytes", observer::number(bytes, "sizeof(registered native type)")},
            {"alignmentBytes", observer::number(alignment, "alignof(registered native type)")},
            {"referenceSlotBytes", observer::not_applicable("native-value")},
            {"arrayStrideBytes", observer::not_applicable("context-not-array-element")},
            {"runtimeReportedObjectBytes", observer::not_applicable("native-value")}})},
        {"members", std::move(fields)}, {"runtimeRegions", Json::array()}, {"limitations", Json::array()},
        {"coverage", Json::object({{"fieldEnumeration", "complete"}, {"extent", "complete"},
            {"occupiedRanges", "complete"}, {"hiddenRegions", "not-applicable"}})}});
}
Json capture(const std::string& run_id) {
    Json::Array types{scalar("i32", sizeof(std::int32_t) * 8, "integer", "signed", "binary-integer"),
        scalar("i16", sizeof(std::int16_t) * 8, "integer", "signed", "binary-integer"),
        scalar("u8", sizeof(std::uint8_t) * 8, "integer", "unsigned", "binary-integer"),
        scalar("char16", sizeof(char16_t) * 8, "character", "not-applicable", "utf16-code-unit"),
        Json::object({{"id", "i32[3]"}, {"displayName", "int32_t[3]"}, {"kind", "array"},
            {"elementTypeRef", "i32"}, {"fixedCount", observer::number(3, "registered C++ array bound")}})};
    for (const auto* name : {"DefaultBoolChar", "ByteBoolChar", "InlineInts"})
        types.push_back(Json::object({{"id", name}, {"displayName", name}, {"kind", "record"}}));
    Json::Array observations;
    observations.push_back(observation("marshal-default", "DefaultBoolChar", sizeof(DefaultBoolChar), alignof(DefaultBoolChar), {
        field("enabled", "i32", offsetof(DefaultBoolChar, enabled), sizeof(DefaultBoolChar::enabled), 0),
        field("letter", "char16", offsetof(DefaultBoolChar, letter), sizeof(DefaultBoolChar::letter), 1),
        field("count", "i32", offsetof(DefaultBoolChar, count), sizeof(DefaultBoolChar::count), 2)}));
    observations.push_back(observation("marshal-byte", "ByteBoolChar", sizeof(ByteBoolChar), alignof(ByteBoolChar), {
        field("enabled", "u8", offsetof(ByteBoolChar, enabled), sizeof(ByteBoolChar::enabled), 0),
        field("letter", "char16", offsetof(ByteBoolChar, letter), sizeof(ByteBoolChar::letter), 1),
        field("count", "i32", offsetof(ByteBoolChar, count), sizeof(ByteBoolChar::count), 2)}));
    auto array_field = field("values", "i32[3]", offsetof(InlineInts, values), sizeof(InlineInts::values), 0);
    array_field["childObservationId"] = "marshal-array/values";
    observations.push_back(observation("marshal-array", "InlineInts", sizeof(InlineInts), alignof(InlineInts), {
        array_field, field("code", "i16", offsetof(InlineInts, code), sizeof(InlineInts::code), 1)}));
    Json::Array elements;
    for (std::size_t i = 0; i < 3; ++i) {
        const auto member_id = std::to_string(i), child = "marshal-array/values/" + member_id;
        auto element = field(member_id, "i32", i * sizeof(std::int32_t), sizeof(std::int32_t), static_cast<int>(i));
        element["childObservationId"] = child; elements.push_back(std::move(element));
        auto child_observation = observation(child, "i32", sizeof(std::int32_t), alignof(std::int32_t), {},
            Json::object({{"kind", "array-element"}, {"hostObservationId", "marshal-array/values"},
                {"hostMemberId", member_id}, {"elementIndex", static_cast<std::int64_t>(i)}}));
        child_observation["metrics"]["arrayStrideBytes"] = observer::number(sizeof(std::int32_t), "sizeof(C++ array element)");
        observations.push_back(std::move(child_observation));
    }
    auto array_observation = observation("marshal-array/values", "i32[3]", sizeof(InlineInts::values), alignof(std::int32_t[3]), elements,
        Json::object({{"kind", "embedded-value"}, {"hostObservationId", "marshal-array"}, {"hostMemberId", "values"}}));
    array_observation["metrics"]["arrayStrideBytes"] = observer::number(sizeof(InlineInts::values), "sizeof(C++ array value): stride between adjacent int32_t[3] values");
    array_observation["instanceShape"] = Json::object({{"length", 3}, {"dimensions", Json::array({3})}});
    observations.push_back(std::move(array_observation));
#if defined(_WIN32)
    const auto os = "windows", abi = "msvc";
#else
    const auto os = "linux", abi = "itanium";
#endif
#if defined(_M_X64) || defined(__x86_64__)
    const auto arch = "x64";
#elif defined(_M_IX86) || defined(__i386__)
    const auto arch = "x86";
#elif defined(_M_ARM64) || defined(__aarch64__)
    const auto arch = "arm64";
#else
    const auto arch = "unknown";
#endif
#if defined(_MSC_VER)
    const auto compiler = "MSVC";
#elif defined(__clang__)
    const auto compiler = "Clang";
#else
    const auto compiler = "GCC";
#endif
    return Json::object({{"schemaVersion", "0.1"}, {"snapshotId", "native-marshalling:" + run_id},
        {"producer", Json::object({{"id", "typelayout-native-marshalling-fixtures"}, {"version", "0.1.1"},
            {"capabilities", Json::array({"native-values", "native-fields", "arrays", "runtime-marshalling-oracle"})}})},
        {"build", Json::object({{"languages", Json::array({"cpp"})}, {"buildId", "native-marshalling:" TYPELAYOUT_MARSHAL_CONFIGURATION}, {"runId", run_id},
            {"configuration", TYPELAYOUT_MARSHAL_CONFIGURATION}, {"sourceRevision", "unknown"}, {"sourceDirty", Json{}},
            {"sourceDigest", "unknown"}, {"artifactDigest", "unknown"},
            {"compiler", Json::object({{"name", compiler}, {"version", observer::compiler_version()}})},
            {"runtime", Json::object({{"name", "none"}, {"version", "none"}})},
            {"target", Json::object({{"os", os}, {"architecture", arch}, {"abi", abi},
                {"pointerBits", static_cast<std::int64_t>(sizeof(void*) * 8)}, {"bitsPerByte", 8},
                {"endian", std::endian::native == std::endian::little ? "little" : "big"}})},
            {"flags", Json::array({"C++20", "default packing"})}, {"dependencies", Json::object({})}})},
        {"typeDescriptors", types}, {"observations", observations}, {"diagnostics", Json::array()},
        {"limitations", Json::array({"Fixed registered native fixtures; layout alone does not prove general ABI compatibility", "source/artifact provenance must be enriched by the orchestrator"})}});
}
}

int capture_main(int argc, char** argv) {
    try {
        std::locale::global(std::locale::classic());
        std::string output, run_id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (i + 1 >= argc) throw std::runtime_error("missing option value: " + arg);
            const std::string value = argv[++i];
            if (value.empty()) throw std::runtime_error("empty option value");
            if (arg == "--output") output = value;
            else if (arg == "--run-id") run_id = value;
            else if (arg == "--configuration") { if (value != TYPELAYOUT_MARSHAL_CONFIGURATION) throw std::runtime_error("configuration-mismatch"); }
            else throw std::runtime_error("unknown option: " + arg);
        }
        auto snapshot = capture(run_id);
        if (output.empty() || output == "-") { snapshot.write(std::cout); std::cout << '\n'; if (!std::cout) throw std::runtime_error("stdout-write-failed"); }
        else {
            const auto utf8_path = std::u8string(reinterpret_cast<const char8_t*>(output.data()), output.size());
            observer::write_new_snapshot(std::filesystem::path(utf8_path), snapshot);
        }
        return 0;
    } catch (const std::exception& error) { std::cerr << "native-marshalling-error: " << error.what() << '\n'; return 3; }
}

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> arguments;
    std::vector<char*> pointers;
    arguments.reserve(argc); pointers.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        const auto count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (!count) { std::cerr << "native-marshalling-error: invalid Unicode argument\n"; return 3; }
        std::string value(static_cast<std::size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, value.data(), count, nullptr, nullptr);
        value.pop_back(); arguments.push_back(std::move(value));
    }
    for (auto& value : arguments) pointers.push_back(value.data());
    return capture_main(argc, pointers.data());
}
#else
int main(int argc, char** argv) { return capture_main(argc, argv); }
#endif
