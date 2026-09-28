#pragma once

#include "json.hpp"
#include <meta>
#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <string_view>
#include <type_traits>
#include <utility>

namespace observer {

template<class T> struct opaque_registration { static constexpr bool value = false; };

template<class T> std::string type_name() {
    return std::string(std::meta::display_string_of(^^T));
}

template<std::meta::info Member> std::string member_name(std::size_t index) {
    if constexpr (std::meta::has_identifier(Member))
        return std::string(std::meta::identifier_of(Member));
    else return "anonymous-" + std::to_string(index);
}

template<class T> consteval std::size_t base_count() {
    return std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size();
}

template<class T, std::size_t I = 0> consteval bool virtual_base() {
    if constexpr (!std::is_class_v<T>) return false;
    else if constexpr (I >= base_count<T>()) return false;
    else {
        constexpr auto base = std::meta::bases_of(^^T, std::meta::access_context::unchecked())[I];
        using B = [:std::meta::type_of(base):];
        return std::meta::is_virtual(base) || virtual_base<B>() || virtual_base<T, I + 1>();
    }
}

class Collector {
    Json::Array descriptors_;
    Json::Array observations_;
    Json::Array diagnostics_;
    std::set<std::string> registered_;
    std::set<std::string> observation_ids_;

    template<class T> Json scalar_representation() {
        auto result = Json::object({{"widthBits", number(sizeof(T) * CHAR_BIT, "sizeof(T)*CHAR_BIT")}});
        constexpr bool character = std::is_same_v<T, char> || std::is_same_v<T, wchar_t> ||
            std::is_same_v<T, char8_t> || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;
        result["category"] = known(std::is_same_v<T, bool> ? "boolean" : character ? "character" :
            std::is_floating_point_v<T> ? "float" : std::is_same_v<T, std::byte> ? "byte" : "integer", "C++ type traits");
        result["signedness"] = known(std::is_integral_v<T> && !std::is_same_v<T, bool> && !character ?
            (std::is_signed_v<T> ? "signed" : "unsigned") : "not-applicable", "integer-category std::is_signed_v<T>; character encoding carries code-unit semantics");
        result["floatingFormat"] = not_applicable("not-floating-point");
        if constexpr (std::is_same_v<T, bool>) {
            constexpr auto zero = std::bit_cast<std::array<unsigned char, sizeof(bool)>>(false);
            constexpr auto one = std::bit_cast<std::array<unsigned char, sizeof(bool)>>(true);
            if constexpr (sizeof(bool) == 1 && zero[0] == 0 && one[0] == 1)
                result["encoding"] = known("bool-0-or-1", "constexpr bit_cast(false/true)");
            else result["encoding"] = unknown("unrecognized-bool-representation");
        } else if constexpr (std::is_same_v<T, char16_t>) result["encoding"] = known("utf16-code-unit", "C++ char16_t");
        else if constexpr (std::is_same_v<T, char32_t>) result["encoding"] = known("utf32-code-unit", "C++ char32_t");
        else if constexpr (std::is_same_v<T, char8_t>) result["encoding"] = known("utf8-code-unit", "C++ char8_t");
        else if constexpr (character) result["encoding"] = unknown("execution-character-encoding-not-measured");
        else if constexpr (std::is_floating_point_v<T>) {
            constexpr bool binary32 = std::numeric_limits<T>::is_iec559 && std::numeric_limits<T>::digits == 24 && sizeof(T) == 4;
            constexpr bool binary64 = std::numeric_limits<T>::is_iec559 && std::numeric_limits<T>::digits == 53 && sizeof(T) == 8;
            result["encoding"] = (binary32 || binary64) ? known("ieee754", "std::numeric_limits<T>") : unknown("unclassified-floating-encoding");
            result["floatingFormat"] = (binary32 || binary64) ? known(binary32 ? "binary32" : "binary64", "std::numeric_limits<T>") : unknown("unclassified-floating-format");
        } else result["encoding"] = known("binary-integer", "C++ integer representation");
        return result;
    }

public:
    template<class T> std::string describe_type() {
        using U = std::remove_cv_t<T>;
        const std::string id = "cpp:" + type_name<U>();
        if (!registered_.insert(id).second) return id;
        Json d = Json::object({{"id", id}, {"displayName", type_name<U>()}});
        if constexpr (opaque_registration<U>::value) {
            d["kind"] = "opaque"; d["opaqueTag"] = opaque_registration<U>::tag;
        } else if constexpr (std::is_void_v<U> || std::is_function_v<U>) {
            d["kind"] = "opaque"; d["opaqueTag"] = "non-object:" + type_name<U>();
        } else if constexpr (std::is_pointer_v<U> || std::is_reference_v<U> || std::is_member_pointer_v<U>) {
            d["kind"] = "reference";
            d["referenceKind"] = std::is_reference_v<U> ? "native-reference" : std::is_member_pointer_v<U> ? "member-pointer" :
                std::is_function_v<std::remove_pointer_t<U>> ? "function-pointer" : "native-pointer";
            if constexpr (std::is_reference_v<U>) d["representation"] = Json::object({{"widthBits", unknown("reference-slot-width-not-portably-measurable")}});
            else d["representation"] = Json::object({{"widthBits", number(sizeof(U) * CHAR_BIT, "sizeof(pointer type)*CHAR_BIT")}});
            if constexpr (std::is_pointer_v<U>) d["targetTypeRef"] = describe_type<std::remove_pointer_t<U>>();
            else if constexpr (std::is_reference_v<U>) d["targetTypeRef"] = describe_type<std::remove_reference_t<U>>();
        } else if constexpr (std::is_array_v<U>) {
            d["kind"] = "array";
            d["elementTypeRef"] = describe_type<std::remove_extent_t<U>>();
            d["fixedCount"] = number(std::extent_v<U>, "std::extent_v<T>");
        } else if constexpr (std::is_enum_v<U>) {
            d["kind"] = "enum"; d["enumUnderlyingTypeRef"] = describe_type<std::underlying_type_t<U>>();
        } else if constexpr (std::is_arithmetic_v<U> || std::is_same_v<U, std::byte>) {
            d["kind"] = "scalar"; d["representation"] = scalar_representation<U>();
        } else d["kind"] = std::is_union_v<U> ? "union" : "record";
        descriptors_.push_back(std::move(d));
        return id;
    }

private:
    template<class Host, std::size_t I> void add_field(Json::Array& members, const std::string& host_id, bool& ranges_complete) {
        constexpr auto member = std::meta::nonstatic_data_members_of(^^Host, std::meta::access_context::unchecked())[I];
        using Field = [:std::meta::type_of(member):];
        constexpr auto position = std::meta::offset_of(member);
        constexpr std::size_t offset = position.bytes * CHAR_BIT + position.bits;
        constexpr bool bitfield = std::meta::is_bit_field(member);
        constexpr bool reused = std::meta::has_attribute(member, ^^[[no_unique_address]]);
        const std::string name = member_name<member>(I);
        Json f = Json::object({{"id", name}, {"displayName", name}, {"declarationOrder", static_cast<std::int64_t>(I)},
            {"role", "field"}, {"typeRef", describe_type<Field>()}, {"offsetBits", number(offset, "std::meta::offset_of(member)")}});
        if constexpr (std::is_reference_v<Field>) {
            f["declaredTypeSizeBits"] = unknown("reference-slot-width-not-portably-measurable");
            f["bitWidth"] = unknown("reference-slot-width-not-portably-measurable");
            f["occupiedRanges"] = unknown("reference-slot-width-not-portably-measurable"); ranges_complete = false;
        } else {
            f["declaredTypeSizeBits"] = number(sizeof(Field) * CHAR_BIT, "sizeof(declared field type)*CHAR_BIT");
            if constexpr (reused) {
                f["bitWidth"] = unknown("potentially-overlapping-subobject");
                f["occupiedRanges"] = unknown("potentially-overlapping-subobject"); ranges_complete = false;
            } else if constexpr (bitfield && std::endian::native != std::endian::little) {
                f["bitWidth"] = number(std::meta::bit_size_of(member), "std::meta::bit_size_of(member)");
                f["occupiedRanges"] = unknown("bit-numbering-not-calibrated-for-target"); ranges_complete = false;
            } else {
                constexpr std::size_t width = bitfield ? std::meta::bit_size_of(member) : sizeof(Field) * CHAR_BIT;
                f["bitWidth"] = number(width, bitfield ? "std::meta::bit_size_of(member)" : "sizeof(field type)*CHAR_BIT");
                f["occupiedRanges"] = range(offset, width, bitfield ? "P2996 little-endian bit offsets (calibrated fixture)" : "offset_of + sizeof ordinary field");
            }
        }
        if constexpr ((std::is_class_v<Field> || std::is_union_v<Field> || std::is_array_v<Field>) && !opaque_registration<Field>::value) {
            const std::string child = host_id + "/" + name;
            f["childObservationId"] = child;
            collect<Field>(child, Json::object({{"kind", "embedded-value"}, {"hostObservationId", host_id}, {"hostMemberId", name}}), reused);
        }
        if constexpr (std::is_union_v<Host>) f["overlapGroup"] = "union:" + host_id;
        members.push_back(std::move(f));
    }

    template<class Host, std::size_t... Is> void add_fields(Json::Array& fields, const std::string& id, bool& complete, std::index_sequence<Is...>) {
        (add_field<Host, Is>(fields, id, complete), ...);
    }

    template<class Host, std::size_t I> void add_base(Json::Array& members, const std::string& host_id, bool& ranges_complete) {
        constexpr auto base = std::meta::bases_of(^^Host, std::meta::access_context::unchecked())[I];
        using Base = [:std::meta::type_of(base):];
        constexpr auto offset = std::meta::offset_of(base).bytes * CHAR_BIT;
        const std::string id = "base-" + std::to_string(I);
        const std::string child = host_id + "/" + id;
        members.push_back(Json::object({{"id", id}, {"displayName", type_name<Base>()},
            {"declarationOrder", static_cast<std::int64_t>(I)}, {"role", "base"}, {"typeRef", describe_type<Base>()},
            {"offsetBits", number(offset, "std::meta::offset_of(base)")},
            {"bitWidth", unknown("base-subobject-extent-not-reported")},
            {"declaredTypeSizeBits", number(sizeof(Base) * CHAR_BIT, "sizeof(Base)*CHAR_BIT")},
            {"occupiedRanges", unknown("base-tail-padding-or-empty-base-may-be-reused")}, {"childObservationId", child}}));
        ranges_complete = false;
        collect<Base>(child, Json::object({{"kind", "embedded-value"}, {"hostObservationId", host_id}, {"hostMemberId", id}}), true);
    }

    template<class Host, std::size_t... Is> void add_bases(Json::Array& fields, const std::string& id, bool& complete, std::index_sequence<Is...>) {
        (add_base<Host, Is>(fields, id, complete), ...);
    }

public:
    template<class T> void collect(const std::string& id, Json context = Json::object({{"kind", "complete-value"}}), bool uncertain_extent = false) {
        if (!observation_ids_.insert(id).second) throw std::runtime_error("duplicate observation ID: " + id);
        Json observation = Json::object({{"id", id}, {"typeId", describe_type<T>()}, {"displayName", type_name<T>()},
            {"status", "ok"}, {"view", "native"}, {"context", std::move(context)},
            {"origin", Json::object({{"kind", "value-start"}, {"extentStartBit", 0}})},
            {"limitations", Json::array()}, {"members", Json::array()}, {"runtimeRegions", Json::array()}});
        auto metrics = Json::object({{"valueSizeBytes", number(sizeof(T), "sizeof(T)")},
            {"standaloneSizeBytes", number(sizeof(T), "sizeof(T)")}, {"alignmentBytes", number(alignof(T), "alignof(T)")},
            {"referenceSlotBytes", not_applicable("value-not-reference")},
            {"arrayStrideBytes", not_applicable("context-not-array-element")},
            {"runtimeReportedObjectBytes", not_applicable("native-value")}});
        if (uncertain_extent) {
            metrics["valueSizeBytes"] = unknown("potentially-overlapping-subobject-extent");
            observation["limitations"].array_items().push_back("standalone size does not establish subobject extent");
        }
        auto coverage = Json::object({{"fieldEnumeration", "complete"}, {"extent", uncertain_extent ? "unknown" : "complete"},
            {"occupiedRanges", "complete"}, {"hiddenRegions", "not-applicable"}});
        if constexpr (virtual_base<T>()) {
            observation["status"] = "unsupported";
            observation["limitations"].array_items().push_back("virtual-inheritance-not-supported");
            for (const auto& key : {"valueSizeBytes", "standaloneSizeBytes", "alignmentBytes", "arrayStrideBytes", "referenceSlotBytes", "runtimeReportedObjectBytes"}) metrics[key] = unknown("virtual-inheritance-not-supported");
            for (const auto& key : {"fieldEnumeration", "extent", "occupiedRanges", "hiddenRegions"}) coverage[key] = "unknown";
            diagnostics_.push_back(Json::object({{"code", "virtual-inheritance-not-supported"},
                {"message", "Current P2996 observer does not locate virtual bases or ABI-owned fields."}, {"observationId", id}}));
        } else if constexpr (opaque_registration<T>::value) {
            coverage["fieldEnumeration"] = "unknown"; coverage["occupiedRanges"] = "unknown"; coverage["hiddenRegions"] = "unknown";
            observation["limitations"].array_items().push_back("opaque-internals-not-observed");
        } else if constexpr (std::is_array_v<T>) {
            using Element = std::remove_extent_t<T>;
            metrics["arrayStrideBytes"] = number(sizeof(Element), "sizeof(array element)");
            observation["instanceShape"] = Json::object({{"length", static_cast<std::int64_t>(std::extent_v<T>)},
                {"dimensions", Json::array({static_cast<std::int64_t>(std::extent_v<T>)})}});
            for (std::size_t i = 0; i < std::extent_v<T>; ++i) {
                const auto member_id = std::to_string(i);
                const auto child_id = id + "/" + member_id;
                Json field = Json::object({{"id", member_id}, {"displayName", "[" + member_id + "]"},
                    {"declarationOrder", static_cast<std::int64_t>(i)}, {"role", "field"}, {"typeRef", describe_type<Element>()},
                    {"offsetBits", number(i * sizeof(Element) * CHAR_BIT, "array-index * sizeof(Element)")},
                    {"bitWidth", number(sizeof(Element) * CHAR_BIT, "sizeof(Element)*CHAR_BIT")},
                    {"declaredTypeSizeBits", number(sizeof(Element) * CHAR_BIT, "sizeof(Element)*CHAR_BIT")},
                    {"occupiedRanges", range(i * sizeof(Element) * CHAR_BIT, sizeof(Element) * CHAR_BIT, "C++ contiguous array elements")},
                    {"childObservationId", child_id}});
                observation["members"].array_items().push_back(std::move(field));
                collect<Element>(child_id, Json::object({{"kind", "array-element"}, {"hostObservationId", id},
                    {"hostMemberId", member_id}, {"elementIndex", static_cast<std::int64_t>(i)}}));
                observations_.back()["metrics"]["arrayStrideBytes"] = number(sizeof(Element), "sizeof(array element)");
            }
        } else if constexpr (std::is_class_v<T> || std::is_union_v<T>) {
            bool complete = true;
            if constexpr (!std::is_union_v<T>) {
                constexpr auto count = std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size();
                add_bases<T>(observation["members"].array_items(), id, complete, std::make_index_sequence<count>{});
            }
            constexpr auto count = std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).size();
            add_fields<T>(observation["members"].array_items(), id, complete, std::make_index_sequence<count>{});
            if (!complete) coverage["occupiedRanges"] = "partial";
            if constexpr (std::is_polymorphic_v<T>) {
                coverage["hiddenRegions"] = "unknown"; coverage["occupiedRanges"] = "partial";
                observation["runtimeRegions"].array_items().push_back(Json::object({{"role", "ABI-owned-polymorphic-state"},
                    {"ranges", unknown("P2996-does-not-report-vptr-locations")}}));
                observation["limitations"].array_items().push_back("polymorphic-hidden-regions-not-observed");
            }
        }
        observation["metrics"] = std::move(metrics);
        observation["coverage"] = std::move(coverage);
        observations_.push_back(std::move(observation));
    }

    Json finish(Json build, const std::string& run_id) {
        static_assert(CHAR_BIT == 8, "observer protocol 0.1 requires 8-bit bytes");
        return Json::object({{"schemaVersion", "0.1"}, {"snapshotId", "native:" + run_id},
            {"producer", Json::object({{"id", "typelayout-native-p2996"}, {"version", "0.1"},
                {"capabilities", Json::array({"native-values", "native-fields", "nested", "arrays", "enums", "bitfields", "bases", "opaque", "nontrivial", "unsupported-diagnostics"})}})},
            {"build", std::move(build)}, {"typeDescriptors", descriptors_}, {"observations", observations_},
            {"diagnostics", diagnostics_}, {"limitations", Json::array({"source/artifact digests require orchestration enrichment before accepting a reproducible baseline"})}});
    }
};

} // namespace observer
