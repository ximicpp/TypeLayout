#ifndef BOOST_TYPELAYOUT_EXAMPLE_XOFFSET_WORLD_DEMO_WORLD_HPP
#define BOOST_TYPELAYOUT_EXAMPLE_XOFFSET_WORLD_DEMO_WORLD_HPP

#include <boost/typelayout.hpp>

#include <xoffsetdatastructure.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace xoffset_world_demo {

template <typename T>
class relative_ptr {
public:
    constexpr relative_ptr() noexcept = default;

    constexpr std::int32_t raw_delta() const noexcept {
        return delta_;
    }

    constexpr explicit operator bool() const noexcept {
        return delta_ != 0;
    }

    T* get() noexcept {
        if (delta_ == 0) {
            return nullptr;
        }
        const auto anchor = reinterpret_cast<std::uintptr_t>(&delta_);
        const auto target = delta_ >= 0
            ? anchor + static_cast<std::uintptr_t>(delta_)
            : anchor - static_cast<std::uintptr_t>(
                -static_cast<std::int64_t>(delta_));
        return reinterpret_cast<T*>(target);
    }

    const T* get() const noexcept {
        if (delta_ == 0) {
            return nullptr;
        }
        const auto anchor = reinterpret_cast<std::uintptr_t>(&delta_);
        const auto target = delta_ >= 0
            ? anchor + static_cast<std::uintptr_t>(delta_)
            : anchor - static_cast<std::uintptr_t>(
                -static_cast<std::int64_t>(delta_));
        return reinterpret_cast<const T*>(target);
    }

    void reset(T* target, std::span<const std::byte> region) {
        if (target == nullptr) {
            delta_ = 0;
            return;
        }

        const auto region_begin =
            reinterpret_cast<std::uintptr_t>(region.data());
        const auto anchor = reinterpret_cast<std::uintptr_t>(&delta_);
        const auto target_address = reinterpret_cast<std::uintptr_t>(target);

        if (region.size() >
            std::numeric_limits<std::uintptr_t>::max() - region_begin) {
            throw std::out_of_range("relative_ptr region end overflows");
        }
        const auto region_end = region_begin + region.size();

        if (anchor < region_begin || anchor > region_end ||
            sizeof(delta_) > region_end - anchor) {
            throw std::out_of_range("relative_ptr anchor is outside region");
        }
        if (target_address < region_begin || target_address > region_end ||
            sizeof(T) > region_end - target_address) {
            throw std::out_of_range("relative_ptr target is outside region");
        }
        if (target_address == anchor) {
            throw std::out_of_range("relative_ptr cannot encode a non-null zero delta");
        }

        if (target_address > anchor) {
            const auto magnitude = target_address - anchor;
            if (magnitude > static_cast<std::uintptr_t>(
                    std::numeric_limits<std::int32_t>::max())) {
                throw std::out_of_range("relative_ptr positive delta is out of range");
            }
            delta_ = static_cast<std::int32_t>(magnitude);
            return;
        }

        const auto magnitude = anchor - target_address;
        constexpr auto negative_limit =
            static_cast<std::uintptr_t>(
                std::numeric_limits<std::int32_t>::max()) + 1;
        if (magnitude > negative_limit) {
            throw std::out_of_range("relative_ptr negative delta is out of range");
        }
        delta_ = magnitude == negative_limit
            ? std::numeric_limits<std::int32_t>::min()
            : -static_cast<std::int32_t>(magnitude);
    }

private:
    std::int32_t delta_ = 0;
};

struct Position {
    std::int32_t x;
    std::int32_t y;
};

enum class EntityKind : std::uint8_t {
    player,
    boss
};

#if defined(TYPELAYOUT_XOFFSET_PACKED_ENTITY)
#pragma pack(push, 1)
#endif
struct Entity {
    std::uint64_t id;
    EntityKind kind;
    Position position;
    std::int32_t hp;
    XOffsetDatastructure::XString name;
    relative_ptr<Entity> owner;
    relative_ptr<Entity> target;
};
#if defined(TYPELAYOUT_XOFFSET_PACKED_ENTITY)
#pragma pack(pop)
#endif

struct WorldSnapshot {
    std::uint64_t tick;
    XOffsetDatastructure::XVector<Entity> entities;
    XOffsetDatastructure::XMap<std::uint64_t, std::uint32_t> entity_index;
    XOffsetDatastructure::XVector<relative_ptr<Entity>> party;
    relative_ptr<Entity> local_player;
};

using EntityIndexEntry =
    XOffsetDatastructure::XKeyValue<std::uint64_t, std::uint32_t>;

} // namespace xoffset_world_demo

XOFFSET_REGISTER_SCHEMA_NAME(
    xoffset_world_demo::WorldSnapshot,
    "boost.typelayout.xoffset_world.v1")

namespace boost {
namespace typelayout {
inline namespace v1 {

template <typename T>
struct source_context_traits<xoffset_world_demo::relative_ptr<T>> {
    static constexpr SourceContext value = SourceContext::same_region;
};

template <>
struct source_context_traits<XOffsetDatastructure::XString> {
    static constexpr SourceContext value = SourceContext::same_region;
};

template <>
struct region_relocation_traits<XOffsetDatastructure::XString> {
    static constexpr bool enabled = true;
};

template <>
struct region_relocation_traits<xoffset_world_demo::Entity> {
    static constexpr bool enabled =
        is_admitted_v<std::uint64_t,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<xoffset_world_demo::EntityKind,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<xoffset_world_demo::Position,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<std::int32_t,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<XOffsetDatastructure::XString,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<xoffset_world_demo::relative_ptr<
                xoffset_world_demo::Entity>,
            TransferProfile::whole_region_relocation>;
};

template <>
struct source_context_traits<
    XOffsetDatastructure::XVector<xoffset_world_demo::Entity>> {
    static constexpr SourceContext value = join_source_context(
        SourceContext::same_region,
        source_context_v<xoffset_world_demo::Entity>);
};

template <>
struct region_relocation_traits<
    XOffsetDatastructure::XVector<xoffset_world_demo::Entity>> {
    static constexpr bool enabled =
        is_admitted_v<xoffset_world_demo::Entity,
            TransferProfile::whole_region_relocation>;
};

template <>
struct source_context_traits<XOffsetDatastructure::XVector<
    xoffset_world_demo::relative_ptr<xoffset_world_demo::Entity>>> {
    static constexpr SourceContext value = join_source_context(
        SourceContext::same_region,
        source_context_v<xoffset_world_demo::relative_ptr<
            xoffset_world_demo::Entity>>);
};

template <>
struct region_relocation_traits<XOffsetDatastructure::XVector<
    xoffset_world_demo::relative_ptr<xoffset_world_demo::Entity>>> {
    static constexpr bool enabled =
        is_admitted_v<xoffset_world_demo::relative_ptr<
                xoffset_world_demo::Entity>,
            TransferProfile::whole_region_relocation>;
};

template <>
struct source_context_traits<
    XOffsetDatastructure::XMap<std::uint64_t, std::uint32_t>> {
    static constexpr SourceContext value = join_source_context(
        SourceContext::same_region,
        join_source_context(
            source_context_v<std::uint64_t>,
            source_context_v<std::uint32_t>));
};

template <>
struct region_relocation_traits<
    XOffsetDatastructure::XMap<std::uint64_t, std::uint32_t>> {
    static constexpr bool enabled =
        is_admitted_v<xoffset_world_demo::EntityIndexEntry,
            TransferProfile::whole_region_relocation>;
};

template <>
struct region_relocation_traits<xoffset_world_demo::WorldSnapshot> {
    static constexpr bool enabled =
        is_admitted_v<std::uint64_t,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<XOffsetDatastructure::XVector<
                xoffset_world_demo::Entity>,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<XOffsetDatastructure::XMap<
                std::uint64_t, std::uint32_t>,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<XOffsetDatastructure::XVector<
                xoffset_world_demo::relative_ptr<xoffset_world_demo::Entity>>,
            TransferProfile::whole_region_relocation> &&
        is_admitted_v<xoffset_world_demo::relative_ptr<
                xoffset_world_demo::Entity>,
            TransferProfile::whole_region_relocation>;
};

} // inline namespace v1
} // namespace typelayout
} // namespace boost

namespace xoffset_world_demo {

template <typename F>
constexpr void for_each_contract_type(F&& fn) {
    fn.template operator()<WorldSnapshot>("WorldSnapshot");
    fn.template operator()<Entity>("Entity");
    fn.template operator()<relative_ptr<Entity>>("EntityRelativePtr");
    fn.template operator()<EntityIndexEntry>("EntityIndexEntry");
}

inline constexpr bool world_contract_admitted_v =
    boost::typelayout::is_admitted_v<WorldSnapshot,
        boost::typelayout::TransferProfile::whole_region_relocation> &&
    boost::typelayout::is_admitted_v<Entity,
        boost::typelayout::TransferProfile::whole_region_relocation> &&
    boost::typelayout::is_admitted_v<relative_ptr<Entity>,
        boost::typelayout::TransferProfile::whole_region_relocation> &&
    boost::typelayout::is_admitted_v<EntityIndexEntry,
        boost::typelayout::TransferProfile::whole_region_relocation>;

using EntityRelativePtr = relative_ptr<Entity>;
static_assert(sizeof(EntityRelativePtr) == 4);
static_assert(alignof(EntityRelativePtr) == 4);
static_assert(std::is_standard_layout_v<EntityRelativePtr>);
static_assert(std::is_trivially_copyable_v<EntityRelativePtr>);
inline constexpr auto entity_relative_ptr_signature =
    boost::typelayout::get_layout_signature<EntityRelativePtr>();
static_assert(!entity_relative_ptr_signature.contains(
    boost::typelayout::FixedString{"O("}));
static_assert(!boost::typelayout::is_admitted_v<
    EntityRelativePtr,
    boost::typelayout::TransferProfile::ordinary_copy>);
static_assert(world_contract_admitted_v);
static_assert(XOffsetDatastructure::is_v1_wire_admitted_v<WorldSnapshot>);

} // namespace xoffset_world_demo

#endif // BOOST_TYPELAYOUT_EXAMPLE_XOFFSET_WORLD_DEMO_WORLD_HPP
