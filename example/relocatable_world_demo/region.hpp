// region.hpp -- Stored representations for the relocatable world demo.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_REGION_HPP
#define BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_REGION_HPP

#include <boost/typelayout.hpp>

#include <cstdint>
#include <type_traits>

namespace relocatable_world_demo {

class RegionBuilder;
class RegionView;
class WorldRegionValidator;
struct WorldRegionAccess;
template <typename T> class relative_ptr;

template <typename T>
class region_handle {
public:
    constexpr region_handle() noexcept = default;
    constexpr ~region_handle() noexcept {}
    constexpr bool is_null() const noexcept { return offset_plus_one_ == 0; }
    constexpr std::uint32_t raw_offset_plus_one() const noexcept {
        return offset_plus_one_;
    }

private:
    constexpr region_handle(const RegionBuilder* owner,
                            std::uint64_t generation,
                            std::uint32_t value) noexcept;
    const RegionBuilder* owner_ = nullptr;
    std::uint64_t generation_ = 0;
    std::uint32_t offset_plus_one_ = 0;
    friend class RegionBuilder;
    friend class WorldRegionValidator;
};

template <typename T>
class relative_ptr {
public:
    constexpr relative_ptr() noexcept = default;
    constexpr relative_ptr(const relative_ptr&) noexcept = default;
    constexpr bool is_null() const noexcept { return offset_plus_one_ == 0; }
    constexpr explicit operator bool() const noexcept { return !is_null(); }
    constexpr std::uint32_t raw_offset_plus_one() const noexcept {
        return offset_plus_one_;
    }

private:
    constexpr relative_ptr& operator=(const relative_ptr&) noexcept = default;
    constexpr relative_ptr& operator=(relative_ptr&&) noexcept = default;
    constexpr void reset_unchecked(region_handle<T> target) noexcept {
        offset_plus_one_ = target.raw_offset_plus_one();
    }
    std::uint32_t offset_plus_one_ = 0;
    friend class RegionBuilder;
};

class region_string {
public:
    constexpr region_string() noexcept = default;
    constexpr region_string(const region_string&) noexcept = default;
    constexpr std::uint32_t size() const noexcept { return size_; }

private:
    constexpr region_string& operator=(const region_string&) noexcept = default;
    constexpr region_string& operator=(region_string&&) noexcept = default;
    relative_ptr<char> data_{};
    std::uint32_t size_ = 0;
    friend class RegionBuilder;
    friend class RegionView;
    friend class WorldRegionValidator;
};

template <typename T>
class region_vector {
public:
    constexpr region_vector() noexcept = default;
    constexpr region_vector(const region_vector&) noexcept = default;
    constexpr std::uint32_t size() const noexcept { return size_; }

private:
    constexpr region_vector& operator=(const region_vector&) noexcept = default;
    constexpr region_vector& operator=(region_vector&&) noexcept = default;
    relative_ptr<T> data_{};
    std::uint32_t size_ = 0;
    friend class RegionBuilder;
    friend class RegionView;
    friend class WorldRegionValidator;
};

template <typename K, typename V>
struct region_key_value {
    K key;
    V value;
};

template <typename K, typename V>
class region_flat_map {
public:
    constexpr region_flat_map() noexcept = default;
    constexpr region_flat_map(const region_flat_map&) noexcept = default;
    constexpr std::uint32_t size() const noexcept { return entries_.size(); }

private:
    constexpr region_flat_map& operator=(const region_flat_map&) noexcept = default;
    constexpr region_flat_map& operator=(region_flat_map&&) noexcept = default;
    region_vector<region_key_value<K, V>> entries_{};
    friend class RegionBuilder;
    friend class RegionView;
    friend class WorldRegionValidator;
};

} // namespace relocatable_world_demo

namespace boost::typelayout::v1 {

template <typename T>
struct source_context_traits<relocatable_world_demo::relative_ptr<T>>
    : std::integral_constant<SourceContext, SourceContext::same_region> {};

template <>
struct source_context_traits<relocatable_world_demo::region_string>
    : std::integral_constant<SourceContext, SourceContext::same_region> {};

template <typename T>
struct source_context_traits<relocatable_world_demo::region_vector<T>>
    : std::integral_constant<SourceContext,
          join_source_context(SourceContext::same_region,
                              source_context_v<T>)> {};

template <>
struct region_relocation_traits<relocatable_world_demo::region_string> {
    static constexpr bool enabled = is_admitted_v<char,
        TransferProfile::whole_region_relocation>;
};

template <typename T>
struct region_relocation_traits<relocatable_world_demo::region_vector<T>> {
    static constexpr bool enabled = is_admitted_v<T,
        TransferProfile::whole_region_relocation>;
};

template <typename K, typename V>
struct region_relocation_traits<
    relocatable_world_demo::region_key_value<K, V>> {
    static constexpr bool enabled =
        is_admitted_v<K, TransferProfile::whole_region_relocation> &&
        is_admitted_v<V, TransferProfile::whole_region_relocation>;
};

template <typename K, typename V>
struct region_relocation_traits<
    relocatable_world_demo::region_flat_map<K, V>> {
    static constexpr bool enabled = is_admitted_v<
        relocatable_world_demo::region_key_value<K, V>,
        TransferProfile::whole_region_relocation>;
};

} // namespace boost::typelayout::v1

#endif // BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_REGION_HPP
