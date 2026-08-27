#include "region.hpp"
#include "region_storage.hpp"
#include "world.hpp"

#include <boost/typelayout.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

using namespace relocatable_world_demo;
using namespace boost::typelayout;

struct DisabledRegionElement {
    std::uint32_t value;
};

struct RegionFixture {
    std::uint32_t scalar;
    region_string name;
    region_vector<std::uint32_t> values;
    relative_ptr<std::uint32_t> selected;
};

struct MapFixture {
    region_flat_map<std::uint64_t, std::uint32_t> index;
};

struct NativePointerFixture {
    std::uint32_t* pointer;
};

namespace boost::typelayout::v1 {
template <>
struct source_context_traits<::DisabledRegionElement>
    : std::integral_constant<SourceContext, SourceContext::same_region> {};

template <>
struct region_relocation_traits<::DisabledRegionElement> {
    static constexpr bool enabled = false;
};
}

static_assert(sizeof(relative_ptr<std::uint32_t>) == 4);
static_assert(alignof(relative_ptr<std::uint32_t>) == 4);
static_assert(std::is_standard_layout_v<relative_ptr<std::uint32_t>>);
static_assert(std::is_trivially_copyable_v<relative_ptr<std::uint32_t>>);
static_assert(std::is_implicit_lifetime_v<relative_ptr<std::uint32_t>>);
static_assert(!std::is_copy_assignable_v<relative_ptr<std::uint32_t>>);
static_assert(!std::is_move_assignable_v<relative_ptr<std::uint32_t>>);
static_assert(sizeof(region_string) == 8);
static_assert(alignof(region_string) == 4);
static_assert(sizeof(region_vector<std::uint32_t>) == 8);
static_assert(alignof(region_vector<std::uint32_t>) == 4);
static_assert(sizeof(region_flat_map<std::uint64_t, std::uint32_t>) == 8);
static_assert(alignof(region_flat_map<std::uint64_t, std::uint32_t>) == 4);
static_assert(std::is_standard_layout_v<region_string>);
static_assert(std::is_trivially_copyable_v<region_string>);
static_assert(std::is_implicit_lifetime_v<region_string>);
static_assert(!std::is_copy_assignable_v<region_string>);
static_assert(!std::is_move_assignable_v<region_string>);
static_assert(std::is_standard_layout_v<region_vector<std::uint32_t>>);
static_assert(std::is_trivially_copyable_v<region_vector<std::uint32_t>>);
static_assert(std::is_implicit_lifetime_v<region_vector<std::uint32_t>>);
static_assert(!std::is_copy_assignable_v<region_vector<std::uint32_t>>);
static_assert(!std::is_move_assignable_v<region_vector<std::uint32_t>>);
static_assert(std::is_standard_layout_v<
    region_flat_map<std::uint64_t, std::uint32_t>>);
static_assert(std::is_trivially_copyable_v<
    region_flat_map<std::uint64_t, std::uint32_t>>);
static_assert(std::is_implicit_lifetime_v<
    region_flat_map<std::uint64_t, std::uint32_t>>);
static_assert(!std::is_copy_assignable_v<
    region_flat_map<std::uint64_t, std::uint32_t>>);
static_assert(!std::is_move_assignable_v<
    region_flat_map<std::uint64_t, std::uint32_t>>);
static_assert(!std::is_copy_assignable_v<RegionFixture>);
static_assert(!std::is_move_assignable_v<RegionFixture>);
static_assert(!std::is_copy_assignable_v<MapFixture>);
static_assert(!std::is_move_assignable_v<MapFixture>);
static_assert(!std::is_trivially_copyable_v<region_handle<std::uint32_t>>);
static_assert(std::is_copy_constructible_v<region_handle<std::uint32_t>>);
static_assert(!std::is_trivially_copyable_v<
    region_array_handle<std::uint32_t>>);
static_assert(std::is_copy_constructible_v<
    region_array_handle<std::uint32_t>>);
static_assert(!std::is_trivially_copyable_v<RegionView>);
static_assert(std::is_copy_constructible_v<RegionView>);
using RegionMapView = basic_region_flat_map_view<
    std::uint64_t,
    std::uint32_t,
    const region_key_value<std::uint64_t, std::uint32_t>>;
static_assert(!std::is_trivially_copyable_v<RegionMapView>);
static_assert(std::is_copy_constructible_v<RegionMapView>);
static_assert(!std::is_copy_constructible_v<RegionBuffer>);
static_assert(!std::is_copy_assignable_v<RegionBuffer>);
static_assert(std::is_move_constructible_v<RegionBuffer>);
static_assert(std::is_move_assignable_v<RegionBuffer>);
static_assert(!std::is_default_constructible_v<RegionView>);
static_assert(!std::is_copy_constructible_v<RegionBuilder>);
static_assert(!std::is_copy_assignable_v<RegionBuilder>);
static_assert(!std::is_move_constructible_v<RegionBuilder>);
static_assert(!std::is_move_assignable_v<RegionBuilder>);
static_assert(region_capacity == 4096);
static_assert(source_context_v<relative_ptr<std::uint32_t>> ==
              SourceContext::same_region);
static_assert(!is_admitted_v<relative_ptr<std::uint32_t>,
    TransferProfile::ordinary_copy>);
static_assert(is_admitted_v<relative_ptr<std::uint32_t>,
    TransferProfile::whole_region_relocation>);
static_assert(!is_admitted_v<region_string,
    TransferProfile::ordinary_copy>);
static_assert(!is_admitted_v<region_vector<std::uint32_t>,
    TransferProfile::ordinary_copy>);
static_assert(!is_admitted_v<
    region_flat_map<std::uint64_t, std::uint32_t>,
    TransferProfile::ordinary_copy>);
static_assert(!is_admitted_v<region_vector<DisabledRegionElement>,
    TransferProfile::whole_region_relocation>);

template <typename Builder>
concept exposes_mutable_get = requires(
    Builder& builder, region_handle<std::uint32_t> handle) {
    builder.get(handle);
};

template <typename Builder>
concept exposes_mutable_at = requires(
    Builder& builder, region_array_handle<std::uint32_t> handle) {
    builder.at(handle, 0);
};

template <typename Builder>
concept accepts_wrong_root = requires(
    Builder& builder, region_handle<std::uint64_t> handle) {
    std::move(builder).finish(handle);
};

template <typename Builder>
concept accepts_world_root = requires(
    Builder& builder, region_handle<WorldSnapshot> handle) {
    std::move(builder).finish(handle);
};

template <typename Builder>
concept ordinarily_sets_descriptor = requires(
    Builder& builder,
    region_handle<WorldSnapshot> handle,
    region_vector<Entity> descriptor) {
    builder.set(handle, &WorldSnapshot::entities, descriptor);
};

template <typename Builder>
concept ordinarily_sets_native_pointer = requires(
    Builder& builder,
    region_handle<NativePointerFixture> handle,
    std::uint32_t* pointer) {
    builder.set(handle, &NativePointerFixture::pointer, pointer);
};

template <typename Builder>
concept ordinarily_sets_string_descriptor = requires(
    Builder& builder,
    region_handle<Entity> handle,
    region_string descriptor) {
    builder.set(handle, &Entity::name, descriptor);
};

template <typename Builder>
concept ordinarily_sets_pointer_descriptor = requires(
    Builder& builder,
    region_handle<Entity> handle,
    relative_ptr<Entity> descriptor) {
    builder.set(handle, &Entity::target, descriptor);
};

template <typename Builder>
concept ordinarily_sets_map_descriptor = requires(
    Builder& builder,
    region_handle<WorldSnapshot> handle,
    region_flat_map<std::uint64_t, std::uint32_t> descriptor) {
    builder.set(handle, &WorldSnapshot::entity_index, descriptor);
};

template <typename Builder>
concept ordinarily_sets_descriptor_element = requires(
    Builder& builder,
    region_array_handle<relative_ptr<std::uint32_t>> destination,
    relative_ptr<std::uint32_t> value) {
    builder.set(destination, 0, value);
};

template <typename Builder>
concept directly_binds_stack_pointer = requires(
    Builder& builder,
    relative_ptr<std::uint32_t>& destination,
    region_handle<std::uint32_t> source) {
    builder.bind(destination, source);
};

template <typename Builder>
concept directly_binds_stack_vector = requires(
    Builder& builder,
    region_vector<std::uint32_t>& destination,
    region_array_handle<std::uint32_t> source) {
    builder.bind(destination, source);
};

template <typename Builder>
concept directly_assigns_stack_string = requires(
    Builder& builder, region_string& destination) {
    builder.assign(destination, "outside");
};

template <typename Builder>
concept directly_binds_stack_map = requires(
    Builder& builder,
    region_flat_map<std::uint64_t, std::uint32_t>& destination,
    region_array_handle<region_key_value<
        std::uint64_t, std::uint32_t>> source) {
    builder.bind(destination, source);
};

static_assert(!exposes_mutable_get<RegionBuilder>);
static_assert(!exposes_mutable_at<RegionBuilder>);
static_assert(!accepts_wrong_root<RegionBuilder>);
static_assert(accepts_world_root<RegionBuilder>);
static_assert(!ordinarily_sets_descriptor<RegionBuilder>);
static_assert(!ordinarily_sets_native_pointer<RegionBuilder>);
static_assert(!ordinarily_sets_string_descriptor<RegionBuilder>);
static_assert(!ordinarily_sets_pointer_descriptor<RegionBuilder>);
static_assert(!ordinarily_sets_map_descriptor<RegionBuilder>);
static_assert(!ordinarily_sets_descriptor_element<RegionBuilder>);
static_assert(!directly_binds_stack_pointer<RegionBuilder>);
static_assert(!directly_binds_stack_vector<RegionBuilder>);
static_assert(!directly_assigns_stack_string<RegionBuilder>);
static_assert(!directly_binds_stack_map<RegionBuilder>);

namespace {

void expect(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Exception, typename Function>
void expect_throws(Function&& function) {
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        return;
    }
    std::abort();
}

template <typename Exception, typename Function>
bool throws_exception(Function&& function) {
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

std::uint32_t read_u32(std::span<const std::byte> bytes,
                       std::size_t offset) {
    expect(offset <= bytes.size() && sizeof(std::uint32_t) <=
                                      bytes.size() - offset);
    std::uint32_t result = 0;
    std::memcpy(&result, bytes.data() + offset, sizeof(result));
    return result;
}

void test_checked_arithmetic() {
    expect(relocatable_world_demo::detail::checked_add(7, 11) == 18);
    expect(relocatable_world_demo::detail::checked_multiply(7, 11) == 77);
    expect(relocatable_world_demo::detail::checked_align_up(9, 8) == 16);

    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    expect_throws<std::length_error>([] {
        static_cast<void>(
            relocatable_world_demo::detail::checked_add(maximum, 1));
    });
    expect_throws<std::length_error>([] {
        static_cast<void>(
            relocatable_world_demo::detail::checked_multiply(maximum, 2));
    });
    expect_throws<std::length_error>([] {
        static_cast<void>(
            relocatable_world_demo::detail::checked_align_up(maximum, 2));
    });
}

void test_builder_and_view_gate() {
    RegionBuilder builder;
    const auto root = builder.make_object<WorldSnapshot>();
    const auto fixture = builder.make_object<RegionFixture>();
    const auto values = builder.make_array<std::uint32_t>(3);
    builder.set(values, 0, std::uint32_t{7});
    builder.set(values, 1, std::uint32_t{11});
    builder.set(values, 2, std::uint32_t{13});
    builder.set(fixture, &RegionFixture::scalar, std::uint32_t{17});
    builder.bind(fixture, &RegionFixture::values, values);
    builder.assign(fixture, &RegionFixture::name, "Hero");
    const auto selected = builder.element_handle(values, 1);
    builder.bind(fixture, &RegionFixture::selected, selected);

    auto buffer = std::move(builder).finish(root);
    const auto* finished_base = buffer.used_bytes().data();
    const auto fixture_offset = fixture.raw_offset_plus_one() - 1;
    const auto values_offset = values.raw_offset_plus_one() - 1;
    expect(read_u32(buffer.used_bytes(), fixture_offset) == 17);
    expect(read_u32(buffer.used_bytes(),
                    fixture_offset + offsetof(RegionFixture, values)) ==
           values.raw_offset_plus_one());
    expect(read_u32(buffer.used_bytes(),
                    fixture_offset + offsetof(RegionFixture, values) + 4) == 3);
    expect(read_u32(buffer.used_bytes(),
                    fixture_offset + offsetof(RegionFixture, selected)) ==
           values.raw_offset_plus_one() + sizeof(std::uint32_t));
    expect(read_u32(buffer.used_bytes(), values_offset) == 7);
    expect(read_u32(buffer.used_bytes(), values_offset + 4) == 11);
    expect(read_u32(buffer.used_bytes(), values_offset + 8) == 13);
    expect(!buffer.is_validated());
    expect_throws<std::logic_error>([&] {
        static_cast<void>(buffer.view());
    });

    auto moved = std::move(buffer);
    expect(moved.used_bytes().data() == finished_base);
    expect(!moved.used_bytes().empty());
    expect(buffer.used_bytes().empty());
    expect(!buffer.is_validated());
    expect_throws<std::logic_error>([&] {
        static_cast<void>(buffer.view());
    });

    RegionBuffer move_assigned;
    move_assigned = std::move(moved);
    expect(move_assigned.used_bytes().data() == finished_base);
    expect(moved.used_bytes().empty());
    expect(!moved.is_validated());
    expect_throws<std::logic_error>([&] {
        static_cast<void>(moved.view());
    });
}

void test_empty_and_capacity_boundaries() {
    RegionBuilder empty_builder;
    const auto root = empty_builder.make_object<WorldSnapshot>();
    const auto fixture = empty_builder.make_object<RegionFixture>();
    const auto empty = empty_builder.make_array<std::uint32_t>(0);
    empty_builder.bind(fixture, &RegionFixture::values, empty);
    auto buffer = std::move(empty_builder).finish(root);
    const auto vector_offset = fixture.raw_offset_plus_one() - 1 +
        offsetof(RegionFixture, values);
    expect(read_u32(buffer.used_bytes(), vector_offset) == 0);
    expect(read_u32(buffer.used_bytes(), vector_offset + 4) == 0);

    RegionBuilder too_large_builder;
    expect_throws<std::length_error>([&] {
        static_cast<void>(too_large_builder.make_array<std::byte>(4097));
    });

    RegionBuilder maximal_count_builder;
    const auto before = maximal_count_builder.make_object<std::uint8_t>();
    expect_throws<std::length_error>([&] {
        static_cast<void>(maximal_count_builder.make_array<std::uint64_t>(
            std::numeric_limits<std::uint32_t>::max()));
    });
    const auto after = maximal_count_builder.make_object<std::uint8_t>();
    expect(before.raw_offset_plus_one() == 1);
    expect(after.raw_offset_plus_one() == 2);
}

void test_handle_provenance() {
    RegionBuilder first_builder;
    const auto first_fixture = first_builder.make_object<RegionFixture>();
    const auto first_value = first_builder.make_object<std::uint32_t>();
    const auto first_values = first_builder.make_array<std::uint32_t>(1);
    const auto first_map = first_builder.make_object<MapFixture>();

    RegionBuilder second_builder;
    const auto second_fixture = second_builder.make_object<RegionFixture>();
    const auto second_value = second_builder.make_object<std::uint32_t>();
    const auto second_values = second_builder.make_array<std::uint32_t>(1);
    const auto second_entries = second_builder.make_array<
        region_key_value<std::uint64_t, std::uint32_t>>(0);
    const auto second_map = second_builder.make_object<MapFixture>();

    expect_throws<std::invalid_argument>([&] {
        first_builder.set(region_handle<RegionFixture>{},
                          &RegionFixture::scalar, std::uint32_t{1});
    });
    expect_throws<std::invalid_argument>([&] {
        first_builder.set(second_fixture,
                          &RegionFixture::scalar, std::uint32_t{1});
    });

    const region_array_handle<std::uint32_t> null_array;
    expect_throws<std::invalid_argument>([&] {
        first_builder.set(null_array, 0, std::uint32_t{1});
    });
    expect_throws<std::invalid_argument>([&] {
        static_cast<void>(first_builder.element_handle(null_array, 0));
    });
    expect_throws<std::invalid_argument>([&] {
        first_builder.set(second_values, 0, std::uint32_t{1});
    });
    expect_throws<std::invalid_argument>([&] {
        static_cast<void>(first_builder.element_handle(second_values, 0));
    });
    expect_throws<std::out_of_range>([&] {
        second_builder.set(second_values, 1, std::uint32_t{1});
    });
    expect_throws<std::out_of_range>([&] {
        static_cast<void>(second_builder.element_handle(second_values, 1));
    });

    expect_throws<std::invalid_argument>([&] {
        first_builder.bind(first_fixture, &RegionFixture::selected,
                           second_value);
    });
    expect_throws<std::invalid_argument>([&] {
        first_builder.bind(second_fixture, &RegionFixture::selected,
                           first_value);
    });
    expect_throws<std::invalid_argument>([&] {
        first_builder.bind(first_fixture, &RegionFixture::values,
                           second_values);
    });
    expect_throws<std::invalid_argument>([&] {
        first_builder.bind(second_fixture, &RegionFixture::values,
                           first_values);
    });
    expect_throws<std::invalid_argument>([&] {
        first_builder.assign(second_fixture, &RegionFixture::name, "outside");
    });
    expect_throws<std::invalid_argument>([&] {
        first_builder.bind(second_map, &MapFixture::index, second_entries);
    });
    expect_throws<std::invalid_argument>([&] {
        first_builder.bind(first_map, &MapFixture::index, second_entries);
    });
}

void test_null_member_pointer_rejection() {
    RegionBuilder set_builder;
    const auto set_fixture = set_builder.make_object<RegionFixture>();
    std::uint32_t RegionFixture::* null_scalar = nullptr;
    const auto set_rejected = throws_exception<std::invalid_argument>([&] {
        set_builder.set(set_fixture, null_scalar, std::uint32_t{1});
    });

    RegionBuilder bind_builder;
    const auto bind_fixture = bind_builder.make_object<RegionFixture>();
    const auto values = bind_builder.make_array<std::uint32_t>(1);
    region_vector<std::uint32_t> RegionFixture::* null_values = nullptr;
    const auto bind_rejected = throws_exception<std::invalid_argument>([&] {
        bind_builder.bind(bind_fixture, null_values, values);
    });

    expect(set_rejected && bind_rejected);
}

void test_finish_closes_builder() {
    RegionBuilder builder;
    const auto root = builder.make_object<WorldSnapshot>();
    const auto fixture = builder.make_object<RegionFixture>();
    const auto values = builder.make_array<std::uint32_t>(1);
    const auto selected = builder.element_handle(values, 0);
    const auto map = builder.make_object<MapFixture>();
    const auto entries = builder.make_array<
        region_key_value<std::uint64_t, std::uint32_t>>(0);
    auto buffer = std::move(builder).finish(root);
    static_cast<void>(buffer);

    expect_throws<std::logic_error>([&] {
        static_cast<void>(builder.make_object<std::uint32_t>());
    });
    expect_throws<std::logic_error>([&] {
        static_cast<void>(builder.make_array<std::uint32_t>(1));
    });
    expect_throws<std::logic_error>([&] {
        builder.set(fixture, &RegionFixture::scalar, std::uint32_t{1});
    });
    expect_throws<std::logic_error>([&] {
        builder.set(values, 0, std::uint32_t{1});
    });
    expect_throws<std::logic_error>([&] {
        static_cast<void>(builder.element_handle(values, 0));
    });
    expect_throws<std::logic_error>([&] {
        builder.bind(fixture, &RegionFixture::values, values);
    });
    expect_throws<std::logic_error>([&] {
        builder.bind(fixture, &RegionFixture::selected, selected);
    });
    expect_throws<std::logic_error>([&] {
        builder.assign(fixture, &RegionFixture::name, "closed");
    });
    expect_throws<std::logic_error>([&] {
        builder.bind(map, &MapFixture::index, entries);
    });
    expect_throws<std::logic_error>([&] {
        static_cast<void>(std::move(builder).finish(root));
    });
}

void test_finish_rejects_invalid_world_roots() {
    RegionBuilder builder;
    expect_throws<std::invalid_argument>([&] {
        static_cast<void>(
            std::move(builder).finish(region_handle<WorldSnapshot>{}));
    });

    RegionBuilder first;
    RegionBuilder second;
    const auto foreign_root = second.make_object<WorldSnapshot>();
    expect_throws<std::invalid_argument>([&] {
        static_cast<void>(std::move(first).finish(foreign_root));
    });
}

} // namespace

int main() {
    test_checked_arithmetic();
    test_builder_and_view_gate();
    test_empty_and_capacity_boundaries();
    test_handle_provenance();
    test_null_member_pointer_rejection();
    test_finish_closes_builder();
    test_finish_rejects_invalid_world_roots();
}
