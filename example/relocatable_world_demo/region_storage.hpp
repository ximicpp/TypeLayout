// region_storage.hpp -- Fixed storage and validated views for the relocatable
// world demo.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef RELOCATABLE_WORLD_DEMO_REGION_STORAGE_HPP
#define RELOCATABLE_WORLD_DEMO_REGION_STORAGE_HPP

#include "region.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace relocatable_world_demo {

inline constexpr std::size_t region_capacity = 4096;

namespace detail {

inline std::size_t checked_add(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        throw std::length_error("region size addition overflow");
    }
    return left + right;
}

inline std::size_t checked_multiply(std::size_t left, std::size_t right) {
    if (left != 0 &&
        right > std::numeric_limits<std::size_t>::max() / left) {
        throw std::length_error("region size multiplication overflow");
    }
    return left * right;
}

inline std::size_t checked_align_up(std::size_t value,
                                    std::size_t alignment) {
    if (alignment == 0) {
        throw std::invalid_argument("region alignment must be non-zero");
    }
    const auto remainder = value % alignment;
    return remainder == 0
        ? value
        : checked_add(value, alignment - remainder);
}

template <typename T>
constexpr void check_stored_type() {
    static_assert(std::is_standard_layout_v<T>,
                  "region objects must be standard-layout types");
    static_assert(std::is_trivially_copyable_v<T>,
                  "region objects must be trivially copyable");
    static_assert(std::is_implicit_lifetime_v<T>,
                  "region objects must be implicit-lifetime types");
    static_assert(alignof(T) <= 64,
                  "region object alignment exceeds storage alignment");
}

} // namespace detail

struct alignas(64) RegionStorage {
    std::byte bytes[region_capacity]{};
};

static_assert(sizeof(RegionStorage) == region_capacity);
static_assert(alignof(RegionStorage) == 64);

template <typename T>
class region_array_handle {
public:
    constexpr region_array_handle() noexcept = default;
    constexpr ~region_array_handle() noexcept {}
    constexpr bool is_null() const noexcept { return count_ == 0; }
    constexpr std::uint32_t raw_offset_plus_one() const noexcept {
        return offset_plus_one_;
    }
    constexpr std::uint32_t size() const noexcept { return count_; }

private:
    constexpr region_array_handle(const RegionBuilder* owner,
                                  std::uint32_t offset_plus_one,
                                  std::uint32_t count) noexcept
        : owner_(owner),
          offset_plus_one_(offset_plus_one),
          count_(count) {}

    const RegionBuilder* owner_ = nullptr;
    std::uint32_t offset_plus_one_ = 0;
    std::uint32_t count_ = 0;
    friend class RegionBuilder;
};

template <typename K, typename V, typename Entry>
class basic_region_flat_map_view {
public:
    using value_type = std::remove_const_t<Entry>;
    using const_iterator = const value_type*;

    constexpr ~basic_region_flat_map_view() noexcept {}

    const_iterator begin() const noexcept { return entries_.data(); }
    const_iterator end() const noexcept {
        return entries_.empty()
            ? entries_.data()
            : entries_.data() + entries_.size();
    }

    const_iterator find(const K& key) const {
        std::size_t first = 0;
        std::size_t count = entries_.size();
        std::less<> less;
        while (count != 0) {
            const auto step = count / 2;
            const auto index = first + step;
            if (less(entries_[index].key, key)) {
                first = index + 1;
                count -= step + 1;
            } else {
                count = step;
            }
        }
        if (first == entries_.size() ||
            less(key, entries_[first].key) ||
            less(entries_[first].key, key)) {
            return end();
        }
        return entries_.data() + first;
    }

private:
    explicit basic_region_flat_map_view(std::span<Entry> entries) noexcept
        : entries_(entries) {}

    std::span<Entry> entries_;
    friend class RegionView;
};

class RegionBuffer {
public:
    RegionBuffer()
        : storage_(std::make_unique<RegionStorage>()) {}

    RegionBuffer(RegionBuffer&& other) noexcept
        : storage_(std::move(other.storage_)),
          used_bytes_(std::exchange(other.used_bytes_, 0)),
          root_offset_(std::exchange(other.root_offset_, 0)),
          state_(std::exchange(other.state_, state::building)) {}

    RegionBuffer& operator=(RegionBuffer&& other) noexcept {
        if (this != &other) {
            storage_ = std::move(other.storage_);
            used_bytes_ = std::exchange(other.used_bytes_, 0);
            root_offset_ = std::exchange(other.root_offset_, 0);
            state_ = std::exchange(other.state_, state::building);
        }
        return *this;
    }

    RegionBuffer(const RegionBuffer&) = delete;
    RegionBuffer& operator=(const RegionBuffer&) = delete;

    bool is_validated() const noexcept {
        return storage_ && state_ == state::validated;
    }

    std::span<const std::byte> used_bytes() const noexcept {
        if (!storage_) {
            return {};
        }
        return {storage_->bytes, used_bytes_};
    }

    RegionView view() const;

private:
    enum class state {
        building,
        constructed_unvalidated,
        copied_bytes_unvalidated,
        validated
    };

    std::unique_ptr<RegionStorage> storage_;
    std::uint32_t used_bytes_ = 0;
    std::uint32_t root_offset_ = 0;
    state state_ = state::building;
    friend class RegionBuilder;
    friend class WorldRegionValidator;
    friend struct WorldRegionAccess;
    friend struct CheckpointRegionAccess;
    friend RegionBuffer load_checkpoint(std::span<const std::byte>);
};

class RegionBuilder {
public:
    RegionBuilder() = default;
    RegionBuilder(const RegionBuilder&) = delete;
    RegionBuilder& operator=(const RegionBuilder&) = delete;
    RegionBuilder(RegionBuilder&&) = delete;
    RegionBuilder& operator=(RegionBuilder&&) = delete;

    template <typename T>
    region_handle<T> make_object() {
        ensure_active();
        detail::check_stored_type<T>();
        const auto offset = reserve(sizeof(T), alignof(T));
        static_cast<void>(std::start_lifetime_as<T>(base() + offset));
        return region_handle<T>(this, encode_offset(offset));
    }

    template <typename T>
    region_array_handle<T> make_array(std::uint32_t count) {
        ensure_active();
        detail::check_stored_type<T>();
        if (count == 0) {
            return region_array_handle<T>(this, 0, 0);
        }
        const auto byte_count = detail::checked_multiply(
            static_cast<std::size_t>(count), sizeof(T));
        const auto offset = reserve(byte_count, alignof(T));
        static_cast<void>(std::start_lifetime_as_array<T>(
            base() + offset, static_cast<std::size_t>(count)));
        return region_array_handle<T>(this, encode_offset(offset), count);
    }

    template <typename T>
    T& get(region_handle<T> handle) {
        ensure_active();
        require_handle(handle);
        return *std::launder(reinterpret_cast<T*>(
            base() + decode_offset(handle.offset_plus_one_)));
    }

    template <typename T>
    T& at(region_array_handle<T> handle, std::uint32_t index) {
        ensure_active();
        require_array_handle(handle);
        if (index >= handle.count_) {
            throw std::out_of_range("region array index is out of range");
        }
        const auto element_offset = detail::checked_add(
            decode_offset(handle.offset_plus_one_),
            detail::checked_multiply(static_cast<std::size_t>(index),
                                     sizeof(T)));
        return *std::launder(reinterpret_cast<T*>(base() + element_offset));
    }

    template <typename T>
    region_handle<T> element_handle(region_array_handle<T> handle,
                                    std::uint32_t index) const {
        ensure_active();
        require_array_handle(handle);
        if (index >= handle.count_) {
            throw std::out_of_range("region array index is out of range");
        }
        const auto element_offset = detail::checked_add(
            decode_offset(handle.offset_plus_one_),
            detail::checked_multiply(static_cast<std::size_t>(index),
                                     sizeof(T)));
        return region_handle<T>(this, encode_offset(element_offset));
    }

    template <typename T>
    void bind(region_vector<T>& destination,
              region_array_handle<T> source) {
        ensure_active();
        require_destination(destination);
        if (source.count_ == 0) {
            destination.data_.reset_unchecked(region_handle<T>{});
            destination.size_ = 0;
            return;
        }
        require_array_handle(source);
        destination.data_.reset_unchecked(
            region_handle<T>(this, source.offset_plus_one_));
        destination.size_ = source.count_;
    }

    template <typename T>
    void bind(relative_ptr<T>& destination, region_handle<T> source) {
        ensure_active();
        require_destination(destination);
        if (!source.is_null() && source.owner_ != this) {
            throw std::invalid_argument(
                "region handle belongs to another builder");
        }
        destination.reset_unchecked(source);
    }

    void assign(region_string& destination, std::string_view text) {
        ensure_active();
        require_destination(destination);
        const auto characters = make_array<char>(
            checked_public_count(text.size()));
        if (!text.empty()) {
            std::memcpy(base() + decode_offset(characters.offset_plus_one_),
                        text.data(), text.size());
        }
        destination.data_.reset_unchecked(text.empty()
            ? region_handle<char>{}
            : region_handle<char>(this, characters.offset_plus_one_));
        destination.size_ = characters.count_;
    }

    template <typename K, typename V>
    void bind(region_flat_map<K, V>& destination,
              region_array_handle<region_key_value<K, V>> source) {
        ensure_active();
        require_destination(destination);
        if (source.count_ == 0) {
            destination.entries_.data_.reset_unchecked(
                region_handle<region_key_value<K, V>>{});
            destination.entries_.size_ = 0;
            return;
        }
        require_array_handle(source);
        destination.entries_.data_.reset_unchecked(
            region_handle<region_key_value<K, V>>(
                this, source.offset_plus_one_));
        destination.entries_.size_ = source.count_;
    }

    template <typename Root>
    RegionBuffer finish(region_handle<Root> root) && {
        ensure_active();
        if (root.is_null() || root.owner_ != this) {
            throw std::invalid_argument("region root handle is null or foreign");
        }
        buffer_.used_bytes_ = checked_storage_count(cursor_);
        buffer_.root_offset_ = checked_storage_count(
            decode_offset(root.offset_plus_one_));
        buffer_.state_ = RegionBuffer::state::constructed_unvalidated;
        active_ = false;
        return std::move(buffer_);
    }

private:
    void ensure_active() const {
        if (!active_ || !buffer_.storage_) {
            throw std::logic_error("region builder is closed");
        }
    }

    std::byte* base() noexcept { return buffer_.storage_->bytes; }
    const std::byte* base() const noexcept { return buffer_.storage_->bytes; }

    std::size_t reserve(std::size_t byte_count, std::size_t alignment) {
        const auto offset = detail::checked_align_up(cursor_, alignment);
        const auto next = detail::checked_add(offset, byte_count);
        if (next > region_capacity) {
            throw std::length_error("region capacity exceeded");
        }
        cursor_ = next;
        return offset;
    }

    static std::size_t decode_offset(std::uint32_t offset_plus_one) {
        if (offset_plus_one == 0) {
            throw std::invalid_argument("null region handle has no offset");
        }
        return static_cast<std::size_t>(offset_plus_one - 1);
    }

    static std::uint32_t encode_offset(std::size_t offset) {
        const auto encoded = detail::checked_add(offset, 1);
        if (encoded > std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("region offset is not representable");
        }
        return static_cast<std::uint32_t>(encoded);
    }

    static std::uint32_t checked_storage_count(std::size_t value) {
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("region size is not representable");
        }
        return static_cast<std::uint32_t>(value);
    }

    static std::uint32_t checked_public_count(std::size_t value) {
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error("region element count is not representable");
        }
        return static_cast<std::uint32_t>(value);
    }

    template <typename T>
    void require_handle(region_handle<T> handle) const {
        if (handle.is_null() || handle.owner_ != this) {
            throw std::invalid_argument("region handle is null or foreign");
        }
    }

    template <typename T>
    void require_array_handle(region_array_handle<T> handle) const {
        if (handle.count_ == 0 || handle.offset_plus_one_ == 0 ||
            handle.owner_ != this) {
            throw std::invalid_argument("region array handle is null or foreign");
        }
    }

    template <typename T>
    void require_destination(const T& destination) const {
        const auto* storage_begin = base();
        const auto* storage_end = storage_begin + cursor_;
        const auto* object_begin = reinterpret_cast<const std::byte*>(
            std::addressof(destination));
        const auto* object_end = object_begin + sizeof(T);
        std::less<const std::byte*> less;
        if (less(object_begin, storage_begin) ||
            less(storage_end, object_end)) {
            throw std::invalid_argument(
                "region destination is outside this builder");
        }
    }

    RegionBuffer buffer_;
    std::size_t cursor_ = 0;
    bool active_ = true;
};

template <typename T>
constexpr region_handle<T>::region_handle(const RegionBuilder* owner,
                                           std::uint32_t value) noexcept
    : owner_(owner), offset_plus_one_(value) {}

class RegionView {
public:
    constexpr ~RegionView() noexcept {}

    template <typename T>
    const T* resolve(const relative_ptr<T>& pointer) const {
        require_descriptor(pointer);
        if (pointer.is_null()) {
            return nullptr;
        }
        const auto offset = static_cast<std::size_t>(
            pointer.raw_offset_plus_one() - 1);
        return std::launder(reinterpret_cast<const T*>(base_ + offset));
    }

    template <typename T>
    std::span<const T> elements(const region_vector<T>& vector) const {
        require_descriptor(vector);
        if (vector.size_ == 0) {
            return {};
        }
        const auto offset = static_cast<std::size_t>(
            vector.data_.raw_offset_plus_one() - 1);
        return {std::launder(reinterpret_cast<const T*>(base_ + offset)),
                vector.size_};
    }

    std::string_view text(const region_string& string) const {
        require_descriptor(string);
        if (string.size_ == 0) {
            return {};
        }
        const auto offset = static_cast<std::size_t>(
            string.data_.raw_offset_plus_one() - 1);
        return {std::launder(reinterpret_cast<const char*>(base_ + offset)),
                string.size_};
    }

    template <typename K, typename V>
    basic_region_flat_map_view<K, V, const region_key_value<K, V>>
    map(const region_flat_map<K, V>& value) const {
        require_descriptor(value);
        return basic_region_flat_map_view<
            K, V, const region_key_value<K, V>>(elements(value.entries_));
    }

private:
    RegionView(const std::byte* base, std::size_t used_bytes) noexcept
        : base_(base), used_bytes_(used_bytes) {}

    template <typename T>
    void require_descriptor(const T& descriptor) const {
        const auto* payload_end = base_ + used_bytes_;
        const auto* object_begin = reinterpret_cast<const std::byte*>(
            std::addressof(descriptor));
        const auto* object_end = object_begin + sizeof(T);
        std::less<const std::byte*> less;
        if (less(object_begin, base_) || less(payload_end, object_end)) {
            throw std::invalid_argument(
                "region descriptor belongs to another buffer");
        }
    }

    const std::byte* base_ = nullptr;
    std::size_t used_bytes_ = 0;
    friend class RegionBuffer;
};

inline RegionView RegionBuffer::view() const {
    if (!storage_ || state_ != state::validated) {
        throw std::logic_error("region buffer has not been validated");
    }
    return RegionView(storage_->bytes, used_bytes_);
}

} // namespace relocatable_world_demo

#endif // RELOCATABLE_WORLD_DEMO_REGION_STORAGE_HPP
