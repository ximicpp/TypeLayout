// region_storage.hpp -- Fixed storage and validated views for the relocatable
// world demo.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_REGION_STORAGE_HPP
#define BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_REGION_STORAGE_HPP

#include "region.hpp"

#include <atomic>
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

template <typename T>
struct is_region_descriptor : std::false_type {};

template <typename T>
struct is_region_descriptor<relative_ptr<T>> : std::true_type {};

template <>
struct is_region_descriptor<region_string> : std::true_type {};

template <typename T>
struct is_region_descriptor<region_vector<T>> : std::true_type {};

template <typename K, typename V>
struct is_region_descriptor<region_flat_map<K, V>> : std::true_type {};

template <typename T>
inline constexpr bool ordinary_writable_v =
    boost::typelayout::is_admitted_v<
        std::remove_cv_t<T>,
        boost::typelayout::TransferProfile::ordinary_copy> &&
    !is_region_descriptor<std::remove_cv_t<T>>::value &&
    !std::is_pointer_v<std::remove_cv_t<T>> &&
    !std::is_member_pointer_v<std::remove_cv_t<T>>;

} // namespace detail

struct alignas(64) RegionStorage {
    std::byte bytes[region_capacity]{};
};

static_assert(sizeof(RegionStorage) == region_capacity);
static_assert(alignof(RegionStorage) == 64);

namespace detail {

// A distinct, fully initialized source for the P0593 memcpy lifetime trigger.
// The demo's closed stored schema guarantees that all-zero is a valid initial
// representation; this is not a generic promise for arbitrary admitted types.
inline constexpr RegionStorage zero_region_storage{};

} // namespace detail

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
                                  std::uint64_t generation,
                                  std::uint32_t offset_plus_one,
                                  std::uint32_t count) noexcept
        : owner_(owner),
          generation_(generation),
          offset_plus_one_(offset_plus_one),
          count_(count) {}

    const RegionBuilder* owner_ = nullptr;
    std::uint64_t generation_ = 0;
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
    friend struct RegionValidationAccess;
};

struct RegionValidationAccess {
    static RegionBuffer copied_buffer(std::span<const std::byte> payload,
                                      std::uint32_t root_offset) {
        if (payload.empty() || payload.size() > region_capacity ||
            root_offset >= payload.size()) {
            throw std::invalid_argument("copied region extent is invalid");
        }
        RegionBuffer buffer;
        // P0593: this distinct-source copy creates the suitable schema objects
        // in the destination; a domain validator gates typed application access.
        std::memcpy(buffer.storage_->bytes, payload.data(), payload.size());
        buffer.used_bytes_ = static_cast<std::uint32_t>(payload.size());
        buffer.root_offset_ = root_offset;
        buffer.state_ = RegionBuffer::state::copied_bytes_unvalidated;
        return buffer;
    }

    static std::byte* base(RegionBuffer& buffer) {
        require_storage(buffer);
        return buffer.storage_->bytes;
    }

    static const std::byte* base(const RegionBuffer& buffer) {
        require_storage(buffer);
        return buffer.storage_->bytes;
    }

    static std::uint32_t used_bytes(const RegionBuffer& buffer) {
        require_storage(buffer);
        return buffer.used_bytes_;
    }

    static std::uint32_t root_offset(const RegionBuffer& buffer) {
        require_storage(buffer);
        return buffer.root_offset_;
    }

    static void require_awaiting_validation(const RegionBuffer& buffer) {
        require_storage(buffer);
        if (buffer.state_ != RegionBuffer::state::constructed_unvalidated &&
            buffer.state_ != RegionBuffer::state::copied_bytes_unvalidated) {
            throw std::logic_error("region buffer is not awaiting validation");
        }
    }

    static void mark_validated(RegionBuffer& buffer) {
        require_awaiting_validation(buffer);
        buffer.state_ = RegionBuffer::state::validated;
    }

private:
    static void require_storage(const RegionBuffer& buffer) {
        if (!buffer.storage_) {
            throw std::logic_error("region buffer has no storage");
        }
    }
};

class RegionBuilder {
public:
    RegionBuilder()
        : generation_(issue_generation()) {}
    RegionBuilder(const RegionBuilder&) = delete;
    RegionBuilder& operator=(const RegionBuilder&) = delete;
    RegionBuilder(RegionBuilder&&) = delete;
    RegionBuilder& operator=(RegionBuilder&&) = delete;

    template <typename T>
    region_handle<T> make_object() {
        ensure_active();
        detail::check_stored_type<T>();
        const auto offset = reserve(sizeof(T), alignof(T));
        create_zero_initialized_objects(offset, sizeof(T));
        return region_handle<T>(this, generation_, encode_offset(offset));
    }

    template <typename T>
    region_array_handle<T> make_array(std::uint32_t count) {
        ensure_active();
        detail::check_stored_type<T>();
        if (count == 0) {
            return region_array_handle<T>(this, generation_, 0, 0);
        }
        const auto byte_count = detail::checked_multiply(
            static_cast<std::size_t>(count), sizeof(T));
        const auto offset = reserve(byte_count, alignof(T));
        create_zero_initialized_objects(offset, byte_count);
        return region_array_handle<T>(
            this, generation_, encode_offset(offset), count);
    }

    template <typename Owner, typename Member, typename Value>
        requires (detail::ordinary_writable_v<Member> &&
                  std::is_same_v<std::remove_cvref_t<Value>,
                                 std::remove_cv_t<Member>> &&
                  std::is_trivially_assignable_v<Member&, Value&&>)
    void set(region_handle<Owner> destination,
             Member Owner::* member,
             Value&& value) {
        require_object_member_destination(destination, member);
        auto& object = resolve_object_unchecked(destination);
        object.*member = std::forward<Value>(value);
    }

    template <typename Owner, typename Member, typename Value>
        requires (detail::ordinary_writable_v<Member> &&
                  std::is_same_v<std::remove_cvref_t<Value>,
                                 std::remove_cv_t<Member>> &&
                  std::is_trivially_assignable_v<Member&, Value&&>)
    void set(region_array_handle<Owner> destination,
             std::uint32_t index,
             Member Owner::* member,
             Value&& value) {
        require_array_member_destination(destination, index, member);
        auto& object = resolve_element_unchecked(destination, index);
        object.*member = std::forward<Value>(value);
    }

    template <typename T, typename Value>
        requires (detail::ordinary_writable_v<T> &&
                  std::is_same_v<std::remove_cvref_t<Value>,
                                 std::remove_cv_t<T>> &&
                  std::is_trivially_assignable_v<T&, Value&&>)
    void set(region_array_handle<T> destination,
             std::uint32_t index,
             Value&& value) {
        require_array_destination(destination, index);
        resolve_element_unchecked(destination, index) =
            std::forward<Value>(value);
    }

    template <typename T>
    region_handle<T> element_handle(region_array_handle<T> handle,
                                    std::uint32_t index) const {
        require_array_destination(handle, index);
        const auto element_offset = detail::checked_add(
            decode_offset(handle.offset_plus_one_),
            detail::checked_multiply(static_cast<std::size_t>(index),
                                     sizeof(T)));
        return region_handle<T>(
            this, generation_, encode_offset(element_offset));
    }

    template <typename Owner, typename T>
    void bind(region_handle<Owner> destination,
              region_vector<T> Owner::* member,
              region_array_handle<T> source) {
        require_object_member_destination(destination, member);
        require_array_source(source);
        auto& object = resolve_object_unchecked(destination);
        bind_vector_unchecked(object.*member, source);
    }

    template <typename Owner, typename T>
    void bind(region_array_handle<Owner> destination,
              std::uint32_t index,
              region_vector<T> Owner::* member,
              region_array_handle<T> source) {
        require_array_member_destination(destination, index, member);
        require_array_source(source);
        auto& object = resolve_element_unchecked(destination, index);
        bind_vector_unchecked(object.*member, source);
    }

    template <typename Owner, typename K, typename V>
    void bind(region_handle<Owner> destination,
              region_flat_map<K, V> Owner::* member,
              region_array_handle<region_key_value<K, V>> source) {
        require_object_member_destination(destination, member);
        require_array_source(source);
        auto& object = resolve_object_unchecked(destination);
        bind_map_unchecked(object.*member, source);
    }

    template <typename Owner, typename K, typename V>
    void bind(region_array_handle<Owner> destination,
              std::uint32_t index,
              region_flat_map<K, V> Owner::* member,
              region_array_handle<region_key_value<K, V>> source) {
        require_array_member_destination(destination, index, member);
        require_array_source(source);
        auto& object = resolve_element_unchecked(destination, index);
        bind_map_unchecked(object.*member, source);
    }

    template <typename Owner, typename T>
    void bind(region_handle<Owner> destination,
              relative_ptr<T> Owner::* member,
              region_handle<T> source) {
        require_object_member_destination(destination, member);
        require_nullable_source(source);
        auto& object = resolve_object_unchecked(destination);
        bind_relative_unchecked(object.*member, source);
    }

    template <typename Owner, typename T>
    void bind(region_array_handle<Owner> destination,
              std::uint32_t index,
              relative_ptr<T> Owner::* member,
              region_handle<T> source) {
        require_array_member_destination(destination, index, member);
        require_nullable_source(source);
        auto& object = resolve_element_unchecked(destination, index);
        bind_relative_unchecked(object.*member, source);
    }

    template <typename T>
    void bind(region_array_handle<relative_ptr<T>> destination,
              std::uint32_t index,
              region_handle<T> source) {
        require_array_destination(destination, index);
        require_nullable_source(source);
        auto& pointer = resolve_element_unchecked(destination, index);
        bind_relative_unchecked(pointer, source);
    }

    template <typename Owner>
    void assign(region_handle<Owner> destination,
                region_string Owner::* member,
                std::string_view text) {
        require_object_member_destination(destination, member);
        auto& object = resolve_object_unchecked(destination);
        assign_string(object.*member, text);
    }

    template <typename Owner>
    void assign(region_array_handle<Owner> destination,
                std::uint32_t index,
                region_string Owner::* member,
                std::string_view text) {
        require_array_member_destination(destination, index, member);
        auto& object = resolve_element_unchecked(destination, index);
        assign_string(object.*member, text);
    }

    template <typename Root>
    RegionBuffer finish(region_handle<Root> root) && {
        detail::check_stored_type<Root>();
        ensure_active();
        if (root.is_null() || root.owner_ != this ||
            root.generation_ != generation_) {
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
    static std::uint64_t issue_generation() {
        static std::atomic<std::uint64_t> next_generation{1};
        auto generation = next_generation.load(std::memory_order_relaxed);
        while (generation != 0) {
            const auto successor =
                generation == std::numeric_limits<std::uint64_t>::max()
                ? 0
                : generation + 1;
            if (next_generation.compare_exchange_weak(
                    generation, successor,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed)) {
                return generation;
            }
        }
        throw std::overflow_error("region builder generation exhausted");
    }

    void ensure_active() const {
        if (!active_ || !buffer_.storage_) {
            throw std::logic_error("region builder is closed");
        }
    }

    std::byte* base() noexcept { return buffer_.storage_->bytes; }
    const std::byte* base() const noexcept { return buffer_.storage_->bytes; }

    void create_zero_initialized_objects(std::size_t offset,
                                         std::size_t byte_count) {
        static_cast<void>(std::memcpy(
            base() + offset, detail::zero_region_storage.bytes, byte_count));
    }

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
        if (handle.is_null() || handle.owner_ != this ||
            handle.generation_ != generation_) {
            throw std::invalid_argument("region handle is null or foreign");
        }
    }

    template <typename T>
    void require_array_handle(region_array_handle<T> handle) const {
        if (handle.count_ == 0 || handle.offset_plus_one_ == 0 ||
            handle.owner_ != this || handle.generation_ != generation_) {
            throw std::invalid_argument("region array handle is null or foreign");
        }
    }

    template <typename T>
    void require_array_source(region_array_handle<T> handle) const {
        if (handle.owner_ != this || handle.generation_ != generation_ ||
            ((handle.count_ == 0) != (handle.offset_plus_one_ == 0))) {
            throw std::invalid_argument("region array handle is foreign");
        }
    }

    template <typename T>
    void require_nullable_source(region_handle<T> handle) const {
        if (!handle.is_null() &&
            (handle.owner_ != this || handle.generation_ != generation_)) {
            throw std::invalid_argument(
                "region handle belongs to another builder");
        }
    }

    template <typename T>
    void require_array_destination(region_array_handle<T> handle,
                                   std::uint32_t index) const {
        ensure_active();
        require_array_handle(handle);
        if (index >= handle.count_) {
            throw std::out_of_range("region array index is out of range");
        }
    }

    template <typename Owner, typename Member>
    void require_object_member_destination(
        region_handle<Owner> destination,
        Member Owner::* member) const {
        ensure_active();
        if (member == nullptr) {
            throw std::invalid_argument("region member pointer is null");
        }
        require_handle(destination);
    }

    template <typename Owner, typename Member>
    void require_array_member_destination(
        region_array_handle<Owner> destination,
        std::uint32_t index,
        Member Owner::* member) const {
        ensure_active();
        if (member == nullptr) {
            throw std::invalid_argument("region member pointer is null");
        }
        require_array_handle(destination);
        if (index >= destination.count_) {
            throw std::out_of_range("region array index is out of range");
        }
    }

    template <typename T>
    T& resolve_object_unchecked(region_handle<T> handle) {
        return *std::launder(reinterpret_cast<T*>(
            base() + decode_offset(handle.offset_plus_one_)));
    }

    template <typename T>
    T& resolve_element_unchecked(region_array_handle<T> handle,
                                 std::uint32_t index) {
        const auto element_offset = detail::checked_add(
            decode_offset(handle.offset_plus_one_),
            detail::checked_multiply(static_cast<std::size_t>(index),
                                     sizeof(T)));
        return *std::launder(reinterpret_cast<T*>(base() + element_offset));
    }

    template <typename T>
    void bind_vector_unchecked(region_vector<T>& destination,
                               region_array_handle<T> source) {
        if (source.count_ == 0) {
            destination.data_.reset_unchecked(region_handle<T>{});
            destination.size_ = 0;
            return;
        }
        destination.data_.reset_unchecked(
            region_handle<T>(
                this, generation_, source.offset_plus_one_));
        destination.size_ = source.count_;
    }

    template <typename K, typename V>
    void bind_map_unchecked(
        region_flat_map<K, V>& destination,
        region_array_handle<region_key_value<K, V>> source) {
        if (source.count_ == 0) {
            destination.entries_.data_.reset_unchecked(
                region_handle<region_key_value<K, V>>{});
            destination.entries_.size_ = 0;
            return;
        }
        destination.entries_.data_.reset_unchecked(
            region_handle<region_key_value<K, V>>(
                this, generation_, source.offset_plus_one_));
        destination.entries_.size_ = source.count_;
    }

    template <typename T>
    void bind_relative_unchecked(relative_ptr<T>& destination,
                                 region_handle<T> source) {
        destination.reset_unchecked(source);
    }

    void assign_string(region_string& destination, std::string_view text) {
        const auto characters = make_array<char>(
            checked_public_count(text.size()));
        if (!text.empty()) {
            std::memcpy(base() + decode_offset(characters.offset_plus_one_),
                        text.data(), text.size());
        }
        destination.data_.reset_unchecked(text.empty()
            ? region_handle<char>{}
            : region_handle<char>(
                this, generation_, characters.offset_plus_one_));
        destination.size_ = characters.count_;
    }

    const std::uint64_t generation_;
    RegionBuffer buffer_;
    std::size_t cursor_ = 0;
    bool active_ = true;
};

template <typename T>
constexpr region_handle<T>::region_handle(const RegionBuilder* owner,
                                           std::uint64_t generation,
                                           std::uint32_t value) noexcept
    : owner_(owner), generation_(generation), offset_plus_one_(value) {}

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

#endif // BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_REGION_STORAGE_HPP
