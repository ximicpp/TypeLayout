#include "dia_collector.hpp"
#include <chrono>
#include <iostream>
#include <locale>

namespace {
struct ComLifetime {
    ComLifetime() { dia_observer::require(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "CoInitializeEx"); }
    ~ComLifetime() { CoUninitialize(); }
};
std::filesystem::path default_dia() {
    std::vector<wchar_t> file(32768);
    const auto length = GetModuleFileNameW(nullptr, file.data(), static_cast<DWORD>(file.size()));
    if (!length || length >= file.size()) throw std::runtime_error("cannot-determine-collector-directory");
    return std::filesystem::path(std::wstring(file.data(), length)).parent_path() / "msdia140.dll";
}
std::string architecture(WORD machine) {
    if (machine == IMAGE_FILE_MACHINE_AMD64) return "x64";
    if (machine == IMAGE_FILE_MACHINE_I386) return "x86";
    if (machine == IMAGE_FILE_MACHINE_ARM64) return "arm64";
    throw std::runtime_error("unsupported-PE-machine: " + std::to_string(machine));
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    using dia_observer::Json;
    try {
        std::locale::global(std::locale::classic());
        std::filesystem::path pe_path, pdb_path, output, dia_path = default_dia();
        std::string requested_configuration;
        std::string run_id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::vector<std::pair<std::string, std::string>> requested_types;
        for (int i = 1; i < argc; ++i) {
            const auto arg = dia_observer::utf8(argv[i]);
            if (arg == "--help") {
                std::cout << "typelayout-msvc --pe IMAGE --pdb SYMBOLS [--output FILE] [--configuration NAME] [--run-id ID] [--dia-dll DLL] [--type ID=QUALIFIED_TYPE]...\n";
                return 0;
            }
            if (i + 1 >= argc) throw std::runtime_error("missing-option-value: " + arg);
            const auto value = dia_observer::utf8(argv[++i]);
            if (value.empty()) throw std::runtime_error("empty-option-value: " + arg);
            if (arg == "--pe") pe_path = argv[i];
            else if (arg == "--pdb") pdb_path = argv[i];
            else if (arg == "--output") output = argv[i];
            else if (arg == "--dia-dll") dia_path = argv[i];
            else if (arg == "--configuration") requested_configuration = value;
            else if (arg == "--run-id") run_id = value;
            else if (arg == "--type") {
                const auto delimiter = value.find('=');
                if (delimiter == std::string::npos || delimiter == 0 || delimiter + 1 == value.size()) throw std::runtime_error("type-option-requires-ID=QUALIFIED_TYPE");
                requested_types.emplace_back(value.substr(0, delimiter), value.substr(delimiter + 1));
            } else throw std::runtime_error("unknown-option: " + arg);
        }
        if (pe_path.empty() || pdb_path.empty()) throw std::runtime_error("--pe and --pdb are required");
        if (requested_types.empty()) requested_types = {{"sample", "fixtures::Sample"}, {"packed", "fixtures::Packed"},
            {"reordered", "fixtures::Reordered"}, {"nested", "fixtures::Nested"}, {"array", "fixtures::Array"}, {"enum", "fixtures::Enum"},
            {"base", "fixtures::Base"}, {"derived", "fixtures::Derived"}, {"bitfields", "fixtures::Bits"}, {"union", "fixtures::Union"},
            {"private", "msvc_fixtures::Private"}, {"polymorphic", "fixtures::Polymorphic"}, {"virtual", "fixtures::Virtual"},
            {"msvc-overlap", "msvc_fixtures::Overlapping"}};
        dia_observer::PeImage image(pe_path);
        ComLifetime lifetime;
        dia_observer::DiaSession session(dia_path, pdb_path, image);
        std::string actual_configuration = "unknown", compiler_version = "unknown";
        Json::Array flags;
        auto marker = image.exported_string("typelayout_fixture_build");
        if (!marker && image.machine == IMAGE_FILE_MACHINE_I386) marker = image.exported_string("_typelayout_fixture_build");
        const std::string prefix = "typelayout-msvc-fixture-v1|";
        if (marker && marker->starts_with(prefix)) {
            const auto separator = marker->find('|', prefix.size());
            if (separator == std::string::npos) throw std::runtime_error("malformed-fixture-build-metadata");
            actual_configuration = marker->substr(prefix.size(), separator - prefix.size());
            compiler_version = marker->substr(separator + 1);
            // These fixture flags are supplied by this independent project's CMake build.
            flags.emplace_back("fixture build metadata: " + actual_configuration);
        }
        if (!requested_configuration.empty() && requested_configuration != actual_configuration)
            throw std::runtime_error("configuration-mismatch: requested " + requested_configuration + ", artifact reports " + actual_configuration);
        std::string compiler_details, compiland_name;
        for (const auto& compiland : dia_observer::children(session.global.Get(), SymTagCompiland)) {
            for (const auto& detail : dia_observer::children(compiland.Get(), SymTagCompilandDetails)) {
                BSTR compiler = nullptr;
                std::string candidate;
                if (detail->get_compilerName(&compiler) == S_OK) { candidate = dia_observer::utf8(compiler); SysFreeString(compiler); }
                DWORD major{}, minor{}, build{};
                if (candidate.find("Compiler") != std::string::npos && detail->get_frontEndMajor(&major) == S_OK && detail->get_frontEndMinor(&minor) == S_OK && detail->get_frontEndBuild(&build) == S_OK && major) {
                    compiler_details = candidate + " " + std::to_string(major) + '.' + std::to_string(minor) + '.' + std::to_string(build);
                    if (compiler_version == "unknown") compiler_version = compiler_details;
                }
                if (!compiler_details.empty()) break;
            }
            if (!compiler_details.empty()) {
                compiland_name = std::filesystem::path(dia_observer::wide(dia_observer::name(compiland.Get()))).filename().string();
                Json::Array actual_commands;
                for (const auto& environment : dia_observer::children(compiland.Get(), SymTagCompilandEnv)) {
                    if (dia_observer::name(environment.Get()) != "cmd") continue;
                    VARIANT value{}; VariantInit(&value);
                    const auto result = environment->get_value(&value);
                    if (result == S_OK && value.vt == VT_BSTR && value.bstrVal) actual_commands.emplace_back(dia_observer::utf8(value.bstrVal));
                    VariantClear(&value);
                }
                if (!actual_commands.empty()) flags = std::move(actual_commands);
                break;
            }
        }
        Json build = Json::object({{"languages", Json::array({"cpp"})}, {"buildId", "pe-pdb:" + dia_observer::guid_text(image.guid) + ':' + std::to_string(image.age)},
            {"runId", run_id}, {"configuration", actual_configuration}, {"sourceRevision", "unknown"}, {"sourceDirty", Json{}},
            {"sourceDigest", "unknown"}, {"artifactDigest", "unknown"},
            {"compiler", Json::object({{"name", "MSVC"}, {"version", compiler_version}})},
            {"runtime", Json::object({{"name", "none"}, {"version", "none"}})},
            {"target", Json::object({{"os", "windows"}, {"architecture", architecture(image.machine)}, {"abi", "msvc"},
                {"pointerBits", static_cast<std::int64_t>(image.pointer_bits)}, {"bitsPerByte", 8}, {"endian", "little"}})},
            {"flags", flags}, {"dependencies", Json::object({{"dia", session.version}, {"pdbGuid", dia_observer::guid_text(image.guid)},
                {"pdbAge", std::to_string(image.age)}, {"compilerDetails", compiler_details}, {"compiland", compiland_name},
                {"sourceState", "not-embedded; sourceDirty=null until orchestration enriches provenance"}})}});
        dia_observer::Collector collector(session);
        // Resolve every request before serializing, so missing type data cannot silently omit a case.
        for (const auto& [id, type_name] : requested_types) collector.collect(collector.find_type(type_name).Get(), id);
        auto snapshot = collector.finish(std::move(build), run_id);
        if (output.empty() || output == L"-") { snapshot.write(std::cout); std::cout << '\n'; if (!std::cout) throw std::runtime_error("stdout-write-failed"); }
        else {
            observer::write_new_snapshot(output, snapshot);
            std::cerr << "Captured validated MSVC PE/PDB snapshot: " << output.string() << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "msvc-capture-error: " << error.what() << '\n'; return 3;
    }
}
