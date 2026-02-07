// Copyright (c) 2024-2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_CORE_SIGNATURE_DETAIL_HPP
#define BOOST_TYPELAYOUT_CORE_SIGNATURE_DETAIL_HPP

#include <boost/typelayout/core/fwd.hpp>
#include <experimental/meta>
#include <type_traits>

namespace boost {
namespace typelayout {

    // Forward declaration
    template <typename T, SignatureMode Mode = SignatureMode::Layout>
    struct TypeSignature;

    // =========================================================================
    // Part 1: P2996 Reflection Meta-Operations
    // =========================================================================

    // Qualified name builder -- P2996 Bloomberg toolchain lacks
    // qualified_name_of, so we walk parent_of chains and join with "::".

    template<std::meta::info R>
    consteval auto qualified_name_for() noexcept {
        using namespace std::meta;
        constexpr auto parent = parent_of(R);
        constexpr std::string_view self = identifier_of(R);
        if constexpr (is_namespace(parent) && has_identifier(parent)) {
            return qualified_name_for<parent>() +
                   FixedString{"::"} +
                   FixedString<self.size() + 1>(self);
        } else {
            return FixedString<self.size() + 1>(self);
        }
    }

    template <typename T>
    consteval std::size_t get_member_count() noexcept {
        return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).size();
    }

    template <typename T>
    consteval std::size_t get_base_count() noexcept {
        return std::meta::bases_of(^^T, std::meta::access_context::unchecked()).size();
    }

    template<std::meta::info Member, std::size_t Index>
    consteval auto get_member_name() noexcept {
        using namespace std::meta;
        if constexpr (has_identifier(Member)) {
            constexpr std::string_view name = identifier_of(Member);
            constexpr size_t NameLen = name.size() + 1;
            return FixedString<NameLen>(name);
        } else {
            return FixedString{"<anon:"} +
                   to_fixed_string(Index) +
                   FixedString{">"};
        }
    }

    template<std::meta::info BaseInfo>
    consteval auto get_base_name() noexcept {
        return qualified_name_for<std::meta::type_of(BaseInfo)>();
    }

    template<typename T>
    consteval auto get_type_qualified_name() noexcept {
        return qualified_name_for<^^T>();
    }

    // =========================================================================
    // Part 2: Definition Signature Engine
    // =========================================================================

    // --- Fields ---

    template<typename T, std::size_t Index>
    consteval auto definition_field_signature() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            return FixedString{"@"} +
                   to_fixed_string(bit_off.bytes) +
                   FixedString{"."} +
                   to_fixed_string(bit_off.bits) +
                   FixedString{"["} + get_member_name<member, Index>() +
                   FixedString{"]:bits<"} +
                   to_fixed_string(bit_size_of(member)) +
                   FixedString{","} +
                   TypeSignature<FieldType, SignatureMode::Definition>::calculate() +
                   FixedString{">"};
        } else {
            return FixedString{"@"} +
                   to_fixed_string(offset_of(member).bytes) +
                   FixedString{"["} + get_member_name<member, Index>() +
                   FixedString{"]:"} +
                   TypeSignature<FieldType, SignatureMode::Definition>::calculate();
        }
    }

    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto definition_field_with_comma() noexcept {
        if constexpr (IsFirst)
            return definition_field_signature<T, Index>();
        else
            return FixedString{","} + definition_field_signature<T, Index>();
    }

    template<typename T, std::size_t... Is>
    consteval auto concatenate_definition_fields(std::index_sequence<Is...>) noexcept {
        return (definition_field_with_comma<T, Is, (Is == 0)>() + ...);
    }

    template <typename T>
    consteval auto definition_fields() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0)
            return FixedString{""};
        else
            return concatenate_definition_fields<T>(std::make_index_sequence<count>{});
    }

    // --- Bases ---

    template<typename T, std::size_t Index>
    consteval auto definition_base_signature() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[Index];
        using BaseType = [:type_of(base_info):];

        if constexpr (is_virtual(base_info))
            return FixedString{"~vbase<"} + get_base_name<base_info>() + FixedString{">:"} +
                   TypeSignature<BaseType, SignatureMode::Definition>::calculate();
        else
            return FixedString{"~base<"} + get_base_name<base_info>() + FixedString{">:"} +
                   TypeSignature<BaseType, SignatureMode::Definition>::calculate();
    }

    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto definition_base_with_comma() noexcept {
        if constexpr (IsFirst)
            return definition_base_signature<T, Index>();
        else
            return FixedString{","} + definition_base_signature<T, Index>();
    }

    template<typename T, std::size_t... Is>
    consteval auto concatenate_definition_bases(std::index_sequence<Is...>) noexcept {
        return (definition_base_with_comma<T, Is, (Is == 0)>() + ...);
    }

    template <typename T>
    consteval auto definition_bases() noexcept {
        constexpr std::size_t count = get_base_count<T>();
        if constexpr (count == 0)
            return FixedString{""};
        else
            return concatenate_definition_bases<T>(std::make_index_sequence<count>{});
    }

    // --- Combined ---

    template <typename T>
    consteval auto definition_content() noexcept {
        constexpr std::size_t bc = get_base_count<T>();
        constexpr std::size_t fc = get_member_count<T>();
        if constexpr (bc == 0 && fc == 0) return FixedString{""};
        else if constexpr (bc == 0) return definition_fields<T>();
        else if constexpr (fc == 0) return definition_bases<T>();
        else return definition_bases<T>() + FixedString{","} + definition_fields<T>();
    }

    // =========================================================================
    // Part 3: Layout Signature Engine
    // =========================================================================

    // Every helper returns a comma-PREFIXED string. The top-level function
    // strips the leading comma via skip_first().

    template <typename T, std::size_t OffsetAdj>
    consteval auto layout_all_prefixed() noexcept;

    template<typename T, std::size_t Index, std::size_t OffsetAdj>
    consteval auto layout_field_with_comma() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            return FixedString{",@"} +
                   to_fixed_string(bit_off.bytes + OffsetAdj) +
                   FixedString{"."} +
                   to_fixed_string(bit_off.bits) +
                   FixedString{":bits<"} +
                   to_fixed_string(bit_size_of(member)) +
                   FixedString{","} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate() +
                   FixedString{">"};
        } else if constexpr (std::is_class_v<FieldType> && !std::is_union_v<FieldType>) {
            constexpr std::size_t field_offset = offset_of(member).bytes + OffsetAdj;
            return layout_all_prefixed<FieldType, field_offset>();
        } else {
            return FixedString{",@"} +
                   to_fixed_string(offset_of(member).bytes + OffsetAdj) +
                   FixedString{":"} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate();
        }
    }

    template <typename T, std::size_t OffsetAdj, std::size_t... Is>
    consteval auto layout_direct_fields_prefixed(std::index_sequence<Is...>) noexcept {
        if constexpr (sizeof...(Is) == 0) return FixedString{""};
        else return (layout_field_with_comma<T, Is, OffsetAdj>() + ...);
    }

    template <typename T, std::size_t BaseIndex, std::size_t OffsetAdj>
    consteval auto layout_one_base_prefixed() noexcept {
        using namespace std::meta;
        constexpr auto base_info = bases_of(^^T, access_context::unchecked())[BaseIndex];
        using BaseType = [:type_of(base_info):];
        return layout_all_prefixed<BaseType, offset_of(base_info).bytes + OffsetAdj>();
    }

    template <typename T, std::size_t OffsetAdj, std::size_t... Is>
    consteval auto layout_bases_prefixed(std::index_sequence<Is...>) noexcept {
        if constexpr (sizeof...(Is) == 0) return FixedString{""};
        else return (layout_one_base_prefixed<T, Is, OffsetAdj>() + ...);
    }

    template <typename T, std::size_t OffsetAdj>
    consteval auto layout_all_prefixed() noexcept {
        constexpr std::size_t bc = get_base_count<T>();
        constexpr std::size_t fc = get_member_count<T>();
        if constexpr (bc == 0 && fc == 0) return FixedString{""};
        else if constexpr (bc == 0) return layout_direct_fields_prefixed<T, OffsetAdj>(std::make_index_sequence<fc>{});
        else if constexpr (fc == 0) return layout_bases_prefixed<T, OffsetAdj>(std::make_index_sequence<bc>{});
        else return layout_bases_prefixed<T, OffsetAdj>(std::make_index_sequence<bc>{}) +
                    layout_direct_fields_prefixed<T, OffsetAdj>(std::make_index_sequence<fc>{});
    }

    template <typename T>
    consteval auto get_layout_content() noexcept {
        return layout_all_prefixed<T, 0>().skip_first();
    }

    // --- Union members: no flattening ---

    template<typename T, std::size_t Index>
    consteval auto layout_union_field() noexcept {
        using namespace std::meta;
        constexpr auto member = nonstatic_data_members_of(^^T, access_context::unchecked())[Index];
        using FieldType = [:type_of(member):];

        if constexpr (is_bit_field(member)) {
            constexpr auto bit_off = offset_of(member);
            return FixedString{"@"} +
                   to_fixed_string(bit_off.bytes) +
                   FixedString{"."} +
                   to_fixed_string(bit_off.bits) +
                   FixedString{":bits<"} +
                   to_fixed_string(bit_size_of(member)) +
                   FixedString{","} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate() +
                   FixedString{">"};
        } else {
            return FixedString{"@"} +
                   to_fixed_string(offset_of(member).bytes) +
                   FixedString{":"} +
                   TypeSignature<FieldType, SignatureMode::Layout>::calculate();
        }
    }

    template<typename T, std::size_t Index, bool IsFirst>
    consteval auto layout_union_field_with_comma() noexcept {
        if constexpr (IsFirst)
            return layout_union_field<T, Index>();
        else
            return FixedString{","} + layout_union_field<T, Index>();
    }

    template<typename T, std::size_t... Is>
    consteval auto concatenate_layout_union_fields(std::index_sequence<Is...>) noexcept {
        return (layout_union_field_with_comma<T, Is, (Is == 0)>() + ...);
    }

    template <typename T>
    consteval auto get_layout_union_content() noexcept {
        constexpr std::size_t count = get_member_count<T>();
        if constexpr (count == 0)
            return FixedString{""};
        else
            return concatenate_layout_union_fields<T>(std::make_index_sequence<count>{});
    }

    // =========================================================================
    // Part 4: TypeSignature Specializations
    // =========================================================================

    template<size_t N>
    consteval auto format_size_align(const char (&name)[N], size_t size, size_t align) noexcept {
        return FixedString{name} + FixedString{"[s:"} +
               to_fixed_string(size) +
               FixedString{",a:"} +
               to_fixed_string(align) +
               FixedString{"]"};
    }

    // Fixed-width integers
    template <SignatureMode Mode> struct TypeSignature<int8_t, Mode>   { static consteval auto calculate() noexcept { return FixedString{"i8[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint8_t, Mode>  { static consteval auto calculate() noexcept { return FixedString{"u8[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<int16_t, Mode>  { static consteval auto calculate() noexcept { return FixedString{"i16[s:2,a:2]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint16_t, Mode> { static consteval auto calculate() noexcept { return FixedString{"u16[s:2,a:2]"}; } };
    template <SignatureMode Mode> struct TypeSignature<int32_t, Mode>  { static consteval auto calculate() noexcept { return FixedString{"i32[s:4,a:4]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint32_t, Mode> { static consteval auto calculate() noexcept { return FixedString{"u32[s:4,a:4]"}; } };
    template <SignatureMode Mode> struct TypeSignature<int64_t, Mode>  { static consteval auto calculate() noexcept { return FixedString{"i64[s:8,a:8]"}; } };
    template <SignatureMode Mode> struct TypeSignature<uint64_t, Mode> { static consteval auto calculate() noexcept { return FixedString{"u64[s:8,a:8]"}; } };

    // Fundamental types (only when distinct from fixed-width aliases)
    template <SignatureMode Mode>
        requires (!std::is_same_v<signed char, int8_t>)
    struct TypeSignature<signed char, Mode> {
        static consteval auto calculate() noexcept { return FixedString{"i8[s:1,a:1]"}; }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<unsigned char, uint8_t>)
    struct TypeSignature<unsigned char, Mode> {
        static consteval auto calculate() noexcept { return FixedString{"u8[s:1,a:1]"}; }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<long, int32_t> && !std::is_same_v<long, int64_t>)
    struct TypeSignature<long, Mode> {
        static consteval auto calculate() noexcept {
            if constexpr (sizeof(long) == 4) return FixedString{"i32[s:4,a:4]"};
            else return FixedString{"i64[s:8,a:8]"};
        }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<unsigned long, uint32_t> && !std::is_same_v<unsigned long, uint64_t>)
    struct TypeSignature<unsigned long, Mode> {
        static consteval auto calculate() noexcept {
            if constexpr (sizeof(unsigned long) == 4) return FixedString{"u32[s:4,a:4]"};
            else return FixedString{"u64[s:8,a:8]"};
        }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<long long, int64_t>)
    struct TypeSignature<long long, Mode> {
        static consteval auto calculate() noexcept { return FixedString{"i64[s:8,a:8]"}; }
    };

    template <SignatureMode Mode>
        requires (!std::is_same_v<unsigned long long, uint64_t>)
    struct TypeSignature<unsigned long long, Mode> {
        static consteval auto calculate() noexcept { return FixedString{"u64[s:8,a:8]"}; }
    };

    // Floating point
    template <SignatureMode Mode> struct TypeSignature<float, Mode>    { static consteval auto calculate() noexcept { return FixedString{"f32[s:4,a:4]"}; } };
    template <SignatureMode Mode> struct TypeSignature<double, Mode>   { static consteval auto calculate() noexcept { return FixedString{"f64[s:8,a:8]"}; } };
    template <SignatureMode Mode> struct TypeSignature<long double, Mode> {
        static consteval auto calculate() noexcept { return format_size_align("f80", sizeof(long double), alignof(long double)); }
    };

    // Character types
    template <SignatureMode Mode> struct TypeSignature<char, Mode>     { static consteval auto calculate() noexcept { return FixedString{"char[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<wchar_t, Mode>  { static consteval auto calculate() noexcept { return format_size_align("wchar", sizeof(wchar_t), alignof(wchar_t)); } };
    template <SignatureMode Mode> struct TypeSignature<char8_t, Mode>  { static consteval auto calculate() noexcept { return FixedString{"char8[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<char16_t, Mode> { static consteval auto calculate() noexcept { return FixedString{"char16[s:2,a:2]"}; } };
    template <SignatureMode Mode> struct TypeSignature<char32_t, Mode> { static consteval auto calculate() noexcept { return FixedString{"char32[s:4,a:4]"}; } };

    // Other fundamentals
    template <SignatureMode Mode> struct TypeSignature<bool, Mode>     { static consteval auto calculate() noexcept { return FixedString{"bool[s:1,a:1]"}; } };
    template <SignatureMode Mode> struct TypeSignature<std::nullptr_t, Mode> { static consteval auto calculate() noexcept { return format_size_align("nullptr", sizeof(std::nullptr_t), alignof(std::nullptr_t)); } };
    template <SignatureMode Mode> struct TypeSignature<std::byte, Mode> { static consteval auto calculate() noexcept { return FixedString{"byte[s:1,a:1]"}; } };

    // Function pointers
    template <typename R, typename... Args, SignatureMode Mode>
    struct TypeSignature<R(*)(Args...), Mode> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args...)), alignof(R(*)(Args...)));
        }
    };
    
    template <typename R, typename... Args, SignatureMode Mode>
    struct TypeSignature<R(*)(Args...) noexcept, Mode> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args...) noexcept), alignof(R(*)(Args...) noexcept));
        }
    };
    
    template <typename R, typename... Args, SignatureMode Mode>
    struct TypeSignature<R(*)(Args..., ...), Mode> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args..., ...)), alignof(R(*)(Args..., ...)));
        }
    };

    template <typename R, typename... Args, SignatureMode Mode>
    struct TypeSignature<R(*)(Args..., ...) noexcept, Mode> {
        static consteval auto calculate() noexcept {
            return format_size_align("fnptr", sizeof(R(*)(Args..., ...) noexcept), alignof(R(*)(Args..., ...) noexcept));
        }
    };

    // CV-qualified: strip and forward
    template <typename T, SignatureMode Mode>
    struct TypeSignature<const T, Mode> {
        static consteval auto calculate() noexcept { return TypeSignature<T, Mode>::calculate(); }
    };
    template <typename T, SignatureMode Mode>
    struct TypeSignature<volatile T, Mode> {
        static consteval auto calculate() noexcept { return TypeSignature<T, Mode>::calculate(); }
    };
    template <typename T, SignatureMode Mode>
    struct TypeSignature<const volatile T, Mode> {
        static consteval auto calculate() noexcept { return TypeSignature<T, Mode>::calculate(); }
    };

    // Pointers and references
    template <typename T, SignatureMode Mode>
    struct TypeSignature<T*, Mode> {
        static consteval auto calculate() noexcept { return format_size_align("ptr", sizeof(T*), alignof(T*)); }
    };
    template <typename T, SignatureMode Mode>
    struct TypeSignature<T&, Mode> {
        static consteval auto calculate() noexcept { return format_size_align("ref", sizeof(T*), alignof(T*)); }
    };
    template <typename T, SignatureMode Mode>
    struct TypeSignature<T&&, Mode> {
        static consteval auto calculate() noexcept { return format_size_align("rref", sizeof(T*), alignof(T*)); }
    };
    template <typename T, typename C, SignatureMode Mode>
    struct TypeSignature<T C::*, Mode> {
        static consteval auto calculate() noexcept { return format_size_align("memptr", sizeof(T C::*), alignof(T C::*)); }
    };

    // Arrays
    template <typename T, SignatureMode Mode>
    struct TypeSignature<T[], Mode> {
        static consteval auto calculate() noexcept {
            static_assert(always_false<T>::value, "Unbounded array T[] has no defined size");
            return FixedString{""};
        }
    };

    template <typename T>
    inline constexpr bool is_byte_element_v =
        std::is_same_v<T, char> || std::is_same_v<T, signed char> ||
        std::is_same_v<T, unsigned char> || std::is_same_v<T, int8_t> ||
        std::is_same_v<T, uint8_t> || std::is_same_v<T, std::byte> ||
        std::is_same_v<T, char8_t>;

    template <typename T, size_t N, SignatureMode Mode>
    struct TypeSignature<T[N], Mode> {
        static consteval auto calculate() noexcept {
            if constexpr (is_byte_element_v<T>) {
                return FixedString{"bytes[s:"} + to_fixed_string(N) + FixedString{",a:1]"};
            } else {
                return FixedString{"array[s:"} + to_fixed_string(sizeof(T[N])) +
                       FixedString{",a:"} + to_fixed_string(alignof(T[N])) +
                       FixedString{"]<"} + TypeSignature<T, Mode>::calculate() +
                       FixedString{","} + to_fixed_string(N) + FixedString{">"};
            }
        }
    };

    // Generic: structs, classes, enums, unions
    template <typename T, SignatureMode Mode>
    struct TypeSignature {
        static consteval auto calculate() noexcept {
            if constexpr (std::is_enum_v<T>) {
                using U = std::underlying_type_t<T>;
                if constexpr (Mode == SignatureMode::Definition) {
                    return FixedString{"enum<"} +
                           get_type_qualified_name<T>() +
                           FixedString{">[s:"} +
                           to_fixed_string(sizeof(T)) +
                           FixedString{",a:"} +
                           to_fixed_string(alignof(T)) +
                           FixedString{"]<"} + TypeSignature<U, Mode>::calculate() + FixedString{">"};
                } else {
                    return FixedString{"enum[s:"} +
                           to_fixed_string(sizeof(T)) +
                           FixedString{",a:"} +
                           to_fixed_string(alignof(T)) +
                           FixedString{"]<"} + TypeSignature<U, Mode>::calculate() + FixedString{">"};
                }
            }
            else if constexpr (std::is_union_v<T>) {
                if constexpr (Mode == SignatureMode::Definition) {
                    return FixedString{"union[s:"} + to_fixed_string(sizeof(T)) +
                           FixedString{",a:"} + to_fixed_string(alignof(T)) +
                           FixedString{"]{"} + definition_fields<T>() + FixedString{"}"};
                } else {
                    return FixedString{"union[s:"} + to_fixed_string(sizeof(T)) +
                           FixedString{",a:"} + to_fixed_string(alignof(T)) +
                           FixedString{"]{"} + get_layout_union_content<T>() + FixedString{"}"};
                }
            }
            else if constexpr (std::is_class_v<T> && !std::is_array_v<T>) {
                if constexpr (Mode == SignatureMode::Layout) {
                    constexpr bool poly = std::is_polymorphic_v<T>;
                    if constexpr (poly) {
                        // vptr occupies pointer_size bytes at an implementation-defined position
                        return FixedString{"record[s:"} +
                               to_fixed_string(sizeof(T)) +
                               FixedString{",a:"} +
                               to_fixed_string(alignof(T)) +
                               FixedString{",vptr]{"} +
                               get_layout_content<T>() +
                               FixedString{"}"};
                    } else {
                        return FixedString{"record[s:"} +
                               to_fixed_string(sizeof(T)) +
                               FixedString{",a:"} +
                               to_fixed_string(alignof(T)) +
                               FixedString{"]{"} +
                               get_layout_content<T>() +
                               FixedString{"}"};
                    }
                } else {
                    // Definition mode: "record" prefix, preserve tree, include names + polymorphic marker
                    constexpr bool poly = std::is_polymorphic_v<T>;
                    auto suffix = [&]() {
                        if constexpr (poly) return FixedString{",polymorphic]{"};
                        else return FixedString{"]{"};
                    }();
                    return FixedString{"record[s:"} +
                           to_fixed_string(sizeof(T)) +
                           FixedString{",a:"} +
                           to_fixed_string(alignof(T)) +
                           suffix + definition_content<T>() + FixedString{"}"};
                }
            }
            else if constexpr (std::is_void_v<T>) {
                static_assert(always_false<T>::value, "void has no layout; use void*");
                return FixedString{""};
            }
            else if constexpr (std::is_function_v<T>) {
                static_assert(always_false<T>::value, "function types have no size; use function pointer");
                return FixedString{""};
            }
            else {
                static_assert(always_false<T>::value, "unsupported type for layout signature");
                return FixedString{""};
            }
        }
    };

} // namespace typelayout
} // namespace boost

#endif // BOOST_TYPELAYOUT_CORE_SIGNATURE_DETAIL_HPP
