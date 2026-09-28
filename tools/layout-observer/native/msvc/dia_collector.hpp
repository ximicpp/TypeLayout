#pragma once
#include "pe.hpp"
#include "../json.hpp"
#include <dia2.h>
#include <cvconst.h>
#include <wrl/client.h>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace dia_observer {
using observer::Json;
using Microsoft::WRL::ComPtr;

inline void require(HRESULT hr, const std::string& operation) {
    if (hr != S_OK) {
        std::ostringstream message;
        message << operation << " (HRESULT 0x" << std::hex << static_cast<unsigned long>(hr) << ')';
        throw std::runtime_error(message.str());
    }
}
inline std::string utf8(const wchar_t* value) {
    if (!value) return {};
    const auto count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
    if (!count) throw std::runtime_error("invalid-unicode-symbol-name");
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, result.data(), count, nullptr, nullptr);
    result.pop_back(); return result;
}
inline std::wstring wide(const std::string& value) {
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, nullptr, 0);
    if (!count) throw std::runtime_error("invalid-utf8-argument");
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, result.data(), count);
    result.pop_back(); return result;
}
inline std::string name(IDiaSymbol* symbol) {
    BSTR text = nullptr;
    if (symbol->get_name(&text) != S_OK) return {};
    const auto result = utf8(text); SysFreeString(text); return result;
}
inline DWORD tag(IDiaSymbol* symbol) {
    DWORD value{}; require(symbol->get_symTag(&value), "DIA get_symTag"); return value;
}
inline std::optional<ULONGLONG> size(IDiaSymbol* symbol) {
    ULONGLONG value{};
    if (symbol->get_length(&value) == S_OK) return value;
    return std::nullopt;
}
inline std::vector<ComPtr<IDiaSymbol>> children(IDiaSymbol* parent, DWORD symbol_tag) {
    ComPtr<IDiaEnumSymbols> enumerator;
    const auto hr = parent->findChildren(static_cast<enum SymTagEnum>(symbol_tag), nullptr, nsNone, &enumerator);
    if (hr == S_FALSE) return {};
    require(hr, "DIA findChildren");
    std::vector<ComPtr<IDiaSymbol>> result;
    for (;;) {
        ComPtr<IDiaSymbol> child;
        ULONG fetched{};
        const auto next = enumerator->Next(1, &child, &fetched);
        if (next == S_FALSE && !fetched) break;
        require(next, "DIA enumerate children");
        if (!fetched) break;
        result.push_back(std::move(child));
    }
    return result;
}
inline std::string guid_text(const GUID& guid) {
    wchar_t text[40]{}; StringFromGUID2(guid, text, 40); return utf8(text);
}
inline std::string file_version(const std::filesystem::path& file) {
    DWORD ignored{};
    const auto count = GetFileVersionInfoSizeW(file.c_str(), &ignored);
    if (!count) return "unknown";
    std::vector<unsigned char> bytes(count);
    if (!GetFileVersionInfoW(file.c_str(), 0, count, bytes.data())) return "unknown";
    VS_FIXEDFILEINFO* version{}; UINT length{};
    if (!VerQueryValueW(bytes.data(), L"\\", reinterpret_cast<void**>(&version), &length)) return "unknown";
    return std::to_string(HIWORD(version->dwFileVersionMS)) + '.' + std::to_string(LOWORD(version->dwFileVersionMS)) + '.' +
        std::to_string(HIWORD(version->dwFileVersionLS)) + '.' + std::to_string(LOWORD(version->dwFileVersionLS));
}

// Registration-free DIA. The DLL remains alive until all COM interfaces release.
class DiaSession {
    HMODULE module_{};
public:
    ComPtr<IDiaDataSource> source;
    ComPtr<IDiaSession> session;
    ComPtr<IDiaSymbol> global;
    std::string version;

    DiaSession(const std::filesystem::path& dll, const std::filesystem::path& pdb, const PeImage& image) {
        module_ = LoadLibraryExW(std::filesystem::absolute(dll).c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!module_) throw std::runtime_error("dia-dll-load-failed: Windows error " + std::to_string(GetLastError()));
        try {
            using FactoryFunction = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);
            auto get_factory = reinterpret_cast<FactoryFunction>(GetProcAddress(module_, "DllGetClassObject"));
            if (!get_factory) throw std::runtime_error("dia-dll-missing-class-factory");
            ComPtr<IClassFactory> factory;
            require(get_factory(__uuidof(DiaSource), __uuidof(IClassFactory), reinterpret_cast<void**>(factory.GetAddressOf())), "DIA class factory");
            require(factory->CreateInstance(nullptr, __uuidof(IDiaDataSource), reinterpret_cast<void**>(source.GetAddressOf())), "DIA instance");
            GUID expected = image.guid;
            require(source->loadAndValidateDataFromPdb(std::filesystem::absolute(pdb).c_str(), &expected, 0, image.age), "pdb-identity-validation-failed");
            require(source->openSession(&session), "DIA openSession");
            require(session->get_globalScope(&global), "DIA globalScope");
            GUID actual{}; DWORD actual_age{};
            require(global->get_guid(&actual), "DIA get PDB GUID");
            require(global->get_age(&actual_age), "DIA get PDB age");
            if (std::memcmp(&actual, &expected, sizeof(GUID)) || actual_age != image.age)
                throw std::runtime_error("pdb-identity-mismatch-after-load");
            BOOL stripped{};
            if (global->get_isStripped(&stripped) == S_OK && stripped)
                throw std::runtime_error("stripped-pdb: full private type information is required");
            version = file_version(dll);
        } catch (...) {
            global.Reset(); session.Reset(); source.Reset(); FreeLibrary(module_); module_ = nullptr; throw;
        }
    }
    ~DiaSession() { global.Reset(); session.Reset(); source.Reset(); if (module_) FreeLibrary(module_); }
    DiaSession(const DiaSession&) = delete;
    DiaSession& operator=(const DiaSession&) = delete;
};

class Collector {
    DiaSession& dia_;
    Json::Array types_, observations_, diagnostics_;
    std::set<std::string> type_ids_, observation_ids_;
    std::map<std::string, ComPtr<IDiaSymbol>> roots_;

    Json fact(Json value, const std::string& method) const {
        return Json::object({{"state", "known"}, {"value", std::move(value)}, {"evidence", Json::object({
            {"kind", "compiler"}, {"method", method}, {"version", "DIA " + dia_.version}, {"inputs", Json::array()}})}});
    }
    Json numeric(ULONGLONG value, const std::string& method) const {
        if (value > static_cast<ULONGLONG>(INT64_MAX)) throw std::runtime_error("DIA numeric fact overflows protocol int64");
        return fact(static_cast<std::int64_t>(value), method);
    }
    Json range(ULONGLONG offset, ULONGLONG width) const {
        return fact(Json::array({Json::object({{"startBit", static_cast<std::int64_t>(offset)},
            {"lengthBits", static_cast<std::int64_t>(width)}})}), "DIA member offset/bitPosition and field/type length");
    }
    std::string id(IDiaSymbol* symbol) const {
        DWORD value{}; require(symbol->get_symIndexId(&value), "DIA symbol ID"); return "dia:" + std::to_string(value);
    }
    ComPtr<IDiaSymbol> type_of(IDiaSymbol* symbol) const {
        ComPtr<IDiaSymbol> type;
        require(symbol->get_type(&type), "DIA missing required type symbol"); return type;
    }
    Json representation(IDiaSymbol* symbol) const {
        DWORD base{}; const auto base_hr = symbol->get_baseType(&base);
        const auto bytes = size(symbol);
        auto result = Json::object({{"widthBits", bytes ? numeric(*bytes * 8, "IDiaSymbol::get_length(type)*8") : observer::unknown("DIA-type-length-unavailable")},
            {"category", observer::unknown("DIA-basic-type-unavailable")}, {"signedness", observer::unknown("DIA-basic-type-unavailable")},
            {"encoding", observer::unknown("DIA-basic-type-unavailable")}, {"floatingFormat", observer::not_applicable("not-floating-point")}});
        if (base_hr != S_OK) return result;
        const bool character = base == btChar || base == btWChar || base == btChar16 || base == btChar32 || base == btChar8;
        if (base == btInt || base == btUInt || base == btLong || base == btULong) {
            result["category"] = fact("integer", "IDiaSymbol::get_baseType");
            result["signedness"] = fact((base == btInt || base == btLong) ? "signed" : "unsigned", "CodeView BasicType");
            result["encoding"] = fact("binary-integer", "CodeView integer + Microsoft target ABI");
        } else if (character) {
            result["category"] = fact("character", "CodeView BasicType");
            result["signedness"] = fact("not-applicable", "character code-unit semantics");
            result["encoding"] = base == btChar16 || base == btWChar ? fact("utf16-code-unit", "Microsoft character ABI") :
                base == btChar32 ? fact("utf32-code-unit", "C++ char32_t") : base == btChar8 ? fact("utf8-code-unit", "C++ char8_t") : observer::unknown("execution-character-encoding-not-recorded");
        } else if (base == btBool) {
            result["category"] = fact("boolean", "CodeView BasicType");
            result["signedness"] = fact("not-applicable", "boolean representation");
            result["encoding"] = bytes == 1 ? fact("bool-0-or-1", "Microsoft native bool ABI") : observer::unknown("unexpected-bool-width");
        } else if (base == btFloat) {
            result["category"] = fact("float", "CodeView BasicType"); result["signedness"] = fact("not-applicable", "floating representation");
            const bool supported = bytes == 4 || bytes == 8;
            result["encoding"] = supported ? fact("ieee754", "Microsoft float/double ABI") : observer::unknown("unclassified-floating-format");
            result["floatingFormat"] = supported ? fact(bytes == 4 ? "binary32" : "binary64", "Microsoft float/double ABI") : observer::unknown("unclassified-floating-format");
        }
        return result;
    }
    bool has_virtual_base(IDiaSymbol* type, std::set<DWORD>& visited) const {
        DWORD identity{}; require(type->get_symIndexId(&identity), "DIA base type ID");
        if (!visited.insert(identity).second) return false;
        for (const auto& base : children(type, SymTagBaseClass)) {
            BOOL is_virtual{};
            if (base->get_virtualBaseClass(&is_virtual) == S_OK && is_virtual) return true;
            const auto actual = type_of(base.Get());
            if (has_virtual_base(actual.Get(), visited)) return true;
        }
        return false;
    }
    bool complex(IDiaSymbol* type) const { return tag(type) == SymTagUDT || tag(type) == SymTagArrayType; }

public:
    explicit Collector(DiaSession& dia) : dia_(dia) {}
    ComPtr<IDiaSymbol> find_type(const std::string& expected) {
        ComPtr<IDiaEnumSymbols> matches;
        require(dia_.global->findChildren(SymTagUDT, wide(expected).c_str(), nsCaseSensitive, &matches), "missing-type: " + expected);
        LONG count{}; require(matches->get_Count(&count), "DIA type match count");
        if (count != 1) throw std::runtime_error("missing-or-ambiguous-type: " + expected + " (matches " + std::to_string(count) + ')');
        ComPtr<IDiaSymbol> found; ULONG fetched{};
        require(matches->Next(1, &found, &fetched), "DIA type match"); return found;
    }

    std::string describe(IDiaSymbol* type) {
        const auto identity = id(type);
        if (!type_ids_.insert(identity).second) return identity;
        auto display = name(type);
        if (display.empty()) display = identity;
        Json descriptor = Json::object({{"id", identity}, {"displayName", display}});
        const auto kind = tag(type);
        if (kind == SymTagBaseType) {
            descriptor["kind"] = "scalar"; descriptor["representation"] = representation(type);
        } else if (kind == SymTagUDT) {
            DWORD udt{}; require(type->get_udtKind(&udt), "DIA UDT kind"); descriptor["kind"] = udt == UdtUnion ? "union" : "record";
        } else if (kind == SymTagArrayType) {
            descriptor["kind"] = "array"; auto element = type_of(type);
            descriptor["elementTypeRef"] = describe(element.Get());
            DWORD count{};
            descriptor["fixedCount"] = type->get_count(&count) == S_OK ? numeric(count, "IDiaSymbol::get_count") : observer::unknown("DIA-array-count-unavailable");
        } else if (kind == SymTagEnum) {
            descriptor["kind"] = "enum";
            auto underlying = type_of(type); descriptor["enumUnderlyingTypeRef"] = describe(underlying.Get());
        } else if (kind == SymTagPointerType) {
            descriptor["kind"] = "reference";
            BOOL reference{}; type->get_reference(&reference);
            auto target = type_of(type);
            descriptor["referenceKind"] = reference ? "native-reference" : tag(target.Get()) == SymTagFunctionType ? "function-pointer" : "native-pointer";
            descriptor["targetTypeRef"] = describe(target.Get());
            const auto bytes = size(type);
            descriptor["representation"] = Json::object({{"widthBits", bytes ? numeric(*bytes * 8, "DIA pointer type length*8") : observer::unknown("DIA-pointer-width-unavailable")}});
        } else if (kind == SymTagTypedef) {
            // DIA normally resolves aliases at get_type; keep explicit alias forwarding if present.
            type_ids_.erase(identity);
            return describe(type_of(type).Get());
        } else { descriptor["kind"] = "opaque"; descriptor["opaqueTag"] = "DIA-symbol-tag:" + std::to_string(kind); }
        types_.push_back(std::move(descriptor)); return identity;
    }

    void collect(IDiaSymbol* type, const std::string& observation_id,
        Json context = Json::object({{"kind", "complete-value"}}), bool uncertain_extent = false, bool array_element = false) {
        if (!observation_ids_.insert(observation_id).second) throw std::runtime_error("duplicate-observation: " + observation_id);
        const auto bytes = size(type);
        const auto symbol_tag = tag(type);
        const auto type_ref = describe(type);
        const auto size_fact = bytes ? numeric(*bytes, "IDiaSymbol::get_length(type)") : observer::unknown("DIA-type-length-unavailable");
        Json metrics = Json::object({{"valueSizeBytes", uncertain_extent ? observer::unknown("base-or-reused-subobject-extent") : size_fact},
            {"standaloneSizeBytes", size_fact}, {"alignmentBytes", observer::unknown("DIA-does-not-report-type-alignment")},
            {"referenceSlotBytes", symbol_tag == SymTagPointerType ? size_fact : observer::not_applicable("not-reference")},
            {"arrayStrideBytes", array_element ? size_fact : observer::not_applicable("context-not-array-element")},
            {"runtimeReportedObjectBytes", observer::not_applicable("native-value")}});
        Json coverage = Json::object({{"fieldEnumeration", "complete"}, {"extent", bytes && !uncertain_extent ? "complete" : "unknown"},
            {"occupiedRanges", "complete"}, {"hiddenRegions", "not-applicable"}});
        Json observation = Json::object({{"id", observation_id}, {"typeId", type_ref}, {"displayName", name(type).empty() ? type_ref : name(type)},
            {"status", "ok"}, {"view", "native"}, {"context", std::move(context)},
            {"origin", Json::object({{"kind", "value-start"}, {"extentStartBit", 0}})},
            {"limitations", Json::array({"DIA does not expose type alignment"})}, {"members", Json::array()}, {"runtimeRegions", Json::array()}});
        std::set<DWORD> visited;
        if (symbol_tag == SymTagUDT && has_virtual_base(type, visited)) {
            observation["status"] = "unsupported";
            observation["limitations"].array_items().push_back("virtual-inheritance-layout-not-supported");
            for (const auto& key : {"valueSizeBytes", "standaloneSizeBytes", "alignmentBytes", "arrayStrideBytes", "referenceSlotBytes", "runtimeReportedObjectBytes"}) metrics[key] = observer::unknown("virtual-inheritance-layout-not-supported");
            for (const auto& key : {"fieldEnumeration", "extent", "occupiedRanges", "hiddenRegions"}) coverage[key] = "unknown";
            diagnostics_.push_back(Json::object({{"code", "virtual-inheritance-layout-not-supported"}, {"observationId", observation_id},
                {"message", "The DIA observer does not resolve virtual-base displacement tables."}}));
        } else if (symbol_tag == SymTagArrayType) {
            auto element = type_of(type); const auto element_size = size(element.Get());
            DWORD count{};
            if (type->get_count(&count) != S_OK || !element_size) {
                coverage["fieldEnumeration"] = "unknown"; coverage["occupiedRanges"] = "unknown";
                observation["limitations"].array_items().push_back("DIA-array-shape-unavailable");
            } else {
                metrics["arrayStrideBytes"] = size_fact;
                observation["instanceShape"] = Json::object({{"length", static_cast<std::int64_t>(count)}, {"dimensions", Json::array({static_cast<std::int64_t>(count)})}});
                if (count > 100000) throw std::runtime_error("array-expansion-limit: explicitly register a bounded fixture");
                for (DWORD i = 0; i < count; ++i) {
                    const auto member_id = std::to_string(i), child = observation_id + '/' + member_id;
                    observation["members"].array_items().push_back(Json::object({{"id", member_id}, {"displayName", '[' + member_id + ']'},
                        {"declarationOrder", static_cast<std::int64_t>(i)}, {"role", "field"}, {"typeRef", describe(element.Get())},
                        {"offsetBits", numeric(i * *element_size * 8, "array index * DIA element length*8")}, {"bitWidth", numeric(*element_size * 8, "DIA element length*8")},
                        {"declaredTypeSizeBits", numeric(*element_size * 8, "DIA element length*8")}, {"occupiedRanges", range(i * *element_size * 8, *element_size * 8)},
                        {"childObservationId", child}}));
                    collect(element.Get(), child, Json::object({{"kind", "array-element"}, {"hostObservationId", observation_id},
                        {"hostMemberId", member_id}, {"elementIndex", static_cast<std::int64_t>(i)}}), false, true);
                }
            }
        } else if (symbol_tag == SymTagUDT) {
            DWORD udt{}; require(type->get_udtKind(&udt), "DIA UDT kind");
            std::size_t order = 0;
            std::set<std::string> class_field_ids;
            for (const auto& base : children(type, SymTagBaseClass)) {
                const auto member_id = "base-" + std::to_string(order++), child = observation_id + '/' + member_id;
                auto actual_type = type_of(base.Get()); const auto base_size = size(actual_type.Get());
                LONG offset{}; const auto hr = base->get_offset(&offset);
                observation["members"].array_items().push_back(Json::object({{"id", member_id}, {"displayName", name(actual_type.Get())},
                    {"declarationOrder", static_cast<std::int64_t>(order - 1)}, {"role", "base"}, {"typeRef", describe(actual_type.Get())},
                    {"offsetBits", hr == S_OK && offset >= 0 ? numeric(static_cast<ULONGLONG>(offset) * 8, "DIA base offset*8") : observer::unknown("DIA-base-offset-unavailable")},
                    {"bitWidth", observer::unknown("base-subobject-extent-not-reported")},
                    {"declaredTypeSizeBits", base_size ? numeric(*base_size * 8, "DIA base standalone length*8") : observer::unknown("DIA-base-length-unavailable")},
                    {"occupiedRanges", observer::unknown("base-tail-padding-or-empty-base-may-be-reused")}, {"childObservationId", child}}));
                coverage["occupiedRanges"] = "partial";
                collect(actual_type.Get(), child, Json::object({{"kind", "embedded-value"}, {"hostObservationId", observation_id}, {"hostMemberId", member_id}}), true);
            }
            for (const auto& field : children(type, SymTagData)) {
                DWORD data_kind{};
                if (field->get_dataKind(&data_kind) != S_OK) { coverage["fieldEnumeration"] = "partial"; continue; }
                if (data_kind != DataIsMember) continue;
                const auto actual_type = type_of(field.Get()); const auto field_size = size(actual_type.Get());
                const auto member_id = name(field.Get()).empty() ? "anonymous-" + std::to_string(order) : name(field.Get());
                LONG offset{}; DWORD location{}; DWORD bit_position{};
                const bool offset_known = field->get_offset(&offset) == S_OK && offset >= 0;
                const bool location_known = field->get_locationType(&location) == S_OK;
                const bool bitfield = location_known && location == LocIsBitField;
                const bool bit_known = !bitfield || field->get_bitPosition(&bit_position) == S_OK;
                const auto bit_length = bitfield ? size(field.Get()) : field_size ? std::optional<ULONGLONG>(*field_size * 8) : std::nullopt;
                const auto bit_offset = static_cast<ULONGLONG>(offset) * 8 + bit_position;
                const bool positioned = offset_known && location_known && bit_known && (location == LocIsThisRel || bitfield);
                const bool class_field = tag(actual_type.Get()) == SymTagUDT;
                const bool empty_class = class_field && children(actual_type.Get(), SymTagData).empty() &&
                    children(actual_type.Get(), SymTagBaseClass).empty() && children(actual_type.Get(), SymTagVTable).empty();
                if (class_field) class_field_ids.insert(member_id);
                Json member = Json::object({{"id", member_id}, {"displayName", member_id}, {"declarationOrder", static_cast<std::int64_t>(order++)},
                    {"role", "field"}, {"typeRef", describe(actual_type.Get())},
                    {"offsetBits", positioned ? numeric(bit_offset, "DIA get_offset*8 + get_bitPosition") : observer::unknown("DIA-member-position-unavailable")},
                    {"bitWidth", empty_class ? observer::unknown("DIA-does-not-identify-empty-subobject-storage") : bit_length ? numeric(*bit_length, bitfield ? "DIA get_length(bitfield), in bits" : "DIA field type length*8") : observer::unknown("DIA-member-width-unavailable")},
                    {"declaredTypeSizeBits", field_size ? numeric(*field_size * 8, "DIA field type length*8") : observer::unknown("DIA-field-type-length-unavailable")},
                    {"occupiedRanges", empty_class ? observer::unknown("DIA-does-not-identify-empty-subobject-storage") : positioned && bit_length ? range(bit_offset, *bit_length) : observer::unknown("DIA-member-range-unavailable")}});
                if (!positioned || !bit_length || empty_class) coverage["occupiedRanges"] = "partial";
                if (udt == UdtUnion) member["overlapGroup"] = "union:" + observation_id;
                if (complex(actual_type.Get())) {
                    const auto child = observation_id + '/' + member_id; member["childObservationId"] = child;
                    collect(actual_type.Get(), child, Json::object({{"kind", "embedded-value"}, {"hostObservationId", observation_id}, {"hostMemberId", member_id}}), empty_class);
                }
                observation["members"].array_items().push_back(std::move(member));
            }
            // DIA lacks a no_unique_address property. A class's standalone tail
            // may intersect another direct member; that is not an exclusive
            // subobject extent. Keep the placement but do not invent occupancy.
            if (udt != UdtUnion) {
                auto& fields = observation["members"].array_items();
                const auto original_fields = fields;
                const auto known_number = [](const Json& fact) -> std::optional<std::int64_t> {
                    if (std::get<std::string>(fact.at("state").value) != "known") return std::nullopt;
                    return std::get<std::int64_t>(fact.at("value").value);
                };
                for (auto& field : fields) {
                    const auto member_id = std::get<std::string>(field.at("id").value);
                    if (!class_field_ids.contains(member_id)) continue;
                    const auto start = known_number(field.at("offsetBits")), width = known_number(field.at("bitWidth"));
                    if (!start || !width) continue;
                    bool overlapping = false;
                    for (const auto& other : original_fields) {
                        if (std::get<std::string>(other.at("id").value) == member_id) continue;
                        const auto other_start = known_number(other.at("offsetBits")), other_width = known_number(other.at("bitWidth"));
                        if (other_start && other_width && *start < *other_start + *other_width && *other_start < *start + *width) overlapping = true;
                    }
                    if (!overlapping) continue;
                    field["bitWidth"] = observer::unknown("potentially-overlapping-class-subobject");
                    field["occupiedRanges"] = observer::unknown("potentially-overlapping-class-subobject");
                    coverage["occupiedRanges"] = "partial";
                    const auto child_id = std::get<std::string>(field.at("childObservationId").value);
                    for (auto& child : observations_) {
                        if (std::get<std::string>(child.at("id").value) != child_id) continue;
                        child["metrics"]["valueSizeBytes"] = observer::unknown("potentially-overlapping-class-subobject");
                        child["coverage"]["extent"] = "unknown";
                        child["limitations"].array_items().push_back("DIA standalone length overlaps sibling storage; subobject extent is unknown");
                    }
                }
            }
            ComPtr<IDiaSymbol> vtable_shape;
            DWORD virtual_slots{};
            const bool has_shape = type->get_virtualTableShape(&vtable_shape) == S_OK && vtable_shape &&
                vtable_shape->get_count(&virtual_slots) == S_OK && virtual_slots > 0;
            if (has_shape || !children(type, SymTagVTable).empty()) {
                coverage["hiddenRegions"] = "unknown"; coverage["occupiedRanges"] = "partial";
                observation["runtimeRegions"].array_items().push_back(Json::object({{"role", "ABI-owned-polymorphic-state"},
                    {"ranges", observer::unknown("DIA-vtable-shape-does-not-prove-vptr-placement")}}));
            }
        } else if (symbol_tag != SymTagBaseType && symbol_tag != SymTagEnum && symbol_tag != SymTagPointerType) {
            coverage["fieldEnumeration"] = "unknown"; coverage["occupiedRanges"] = "unknown";
            observation["limitations"].array_items().push_back("DIA-symbol-kind-not-expanded");
        }
        observation["metrics"] = std::move(metrics); observation["coverage"] = std::move(coverage);
        observations_.push_back(std::move(observation));
    }

    Json finish(Json build, const std::string& run_id) {
        return Json::object({{"schemaVersion", "0.1"}, {"snapshotId", "msvc-dia:" + run_id},
            {"producer", Json::object({{"id", "typelayout-msvc-dia"}, {"version", "0.1.1"},
                {"capabilities", Json::array({"native-values", "native-fields", "nested", "arrays", "enums", "bitfields", "bases", "private-fields", "pdb-identity-validation", "unsupported-diagnostics"})}})},
            {"build", std::move(build)}, {"typeDescriptors", types_}, {"observations", observations_}, {"diagnostics", diagnostics_},
            {"limitations", Json::array({"alignment is not supplied by DIA", "source/artifact digests require orchestration enrichment before accepting a reproducible baseline", "DIA describes emitted PDB type data; compiler options not embedded by the build remain unavailable"})}});
    }
};
} // namespace dia_observer
