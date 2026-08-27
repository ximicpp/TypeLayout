#include "region.hpp"
#include "region_storage.hpp"

#include <boost/typelayout.hpp>

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace relocatable_world_demo;
using namespace boost::typelayout;

struct DisabledRegionElement {
    std::uint32_t value;
};

struct RegionFixture {
    region_string name;
    region_vector<std::uint32_t> values;
    relative_ptr<std::uint32_t> selected;
};

struct MapFixture {
    region_flat_map<std::uint64_t, std::uint32_t> index;
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

std::array<std::uint32_t, 2> raw_vector_words(
    const region_vector<std::uint32_t>& value) {
    return std::bit_cast<std::array<std::uint32_t, 2>>(value);
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
    const auto root = builder.make_object<RegionFixture>();
    const auto* initial_root = &builder.get(root);
    const auto values = builder.make_array<std::uint32_t>(3);
    builder.at(values, 0) = 7;
    builder.at(values, 1) = 11;
    builder.at(values, 2) = 13;
    builder.bind(builder.get(root).values, values);
    builder.assign(builder.get(root).name, "Hero");
    const auto selected = builder.element_handle(values, 1);
    builder.bind(builder.get(root).selected, selected);
    expect(&builder.get(root) == initial_root);
    expect(builder.get(root).selected.raw_offset_plus_one() != 0);

    auto buffer = std::move(builder).finish(root);
    const auto* finished_base = buffer.used_bytes().data();
    expect(finished_base == reinterpret_cast<const std::byte*>(initial_root));
    expect(reinterpret_cast<std::uintptr_t>(finished_base) % 64 == 0);
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
    const auto empty_root = empty_builder.make_object<RegionFixture>();
    const auto empty = empty_builder.make_array<std::uint32_t>(0);
    empty_builder.bind(empty_builder.get(empty_root).values, empty);
    expect(empty_builder.get(empty_root).values.size() == 0);
    expect(raw_vector_words(empty_builder.get(empty_root).values) ==
           std::array<std::uint32_t, 2>{0, 0});

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

void test_handle_and_destination_provenance() {
    RegionBuilder first_builder;
    const auto first_fixture = first_builder.make_object<RegionFixture>();
    const auto first_value = first_builder.make_object<std::uint32_t>();

    RegionBuilder second_builder;
    const auto second_value = second_builder.make_object<std::uint32_t>();
    const auto second_values = second_builder.make_array<std::uint32_t>(1);

    expect_throws<std::invalid_argument>([&] {
        first_builder.bind(first_builder.get(first_fixture).selected,
                           second_value);
    });
    expect_throws<std::invalid_argument>([&] {
        first_builder.bind(first_builder.get(first_fixture).values,
                           second_values);
    });

    relative_ptr<std::uint32_t> stack_pointer;
    expect_throws<std::invalid_argument>([&] {
        first_builder.bind(stack_pointer, first_value);
    });
    expect_throws<std::invalid_argument>([&] {
        second_builder.bind(stack_pointer, second_value);
    });

    region_vector<std::uint32_t> stack_vector;
    region_string stack_string;
    region_flat_map<std::uint64_t, std::uint32_t> stack_map;
    const auto second_entries = second_builder.make_array<
        region_key_value<std::uint64_t, std::uint32_t>>(0);
    expect_throws<std::invalid_argument>([&] {
        second_builder.bind(stack_vector, second_values);
    });
    expect_throws<std::invalid_argument>([&] {
        second_builder.assign(stack_string, "outside");
    });
    expect_throws<std::invalid_argument>([&] {
        second_builder.bind(stack_map, second_entries);
    });
}

void test_finish_closes_builder() {
    RegionBuilder builder;
    const auto root = builder.make_object<RegionFixture>();
    const auto values = builder.make_array<std::uint32_t>(1);
    const auto selected = builder.element_handle(values, 0);
    const auto entries = builder.make_array<
        region_key_value<std::uint64_t, std::uint32_t>>(0);
    auto* fixture = &builder.get(root);
    auto buffer = std::move(builder).finish(root);
    static_cast<void>(buffer);

    expect_throws<std::logic_error>([&] {
        static_cast<void>(builder.make_object<std::uint32_t>());
    });
    expect_throws<std::logic_error>([&] {
        static_cast<void>(builder.make_array<std::uint32_t>(1));
    });
    expect_throws<std::logic_error>([&] {
        static_cast<void>(builder.get(root));
    });
    expect_throws<std::logic_error>([&] {
        static_cast<void>(builder.at(values, 0));
    });
    expect_throws<std::logic_error>([&] {
        static_cast<void>(builder.element_handle(values, 0));
    });
    expect_throws<std::logic_error>([&] {
        builder.bind(fixture->values, values);
    });
    expect_throws<std::logic_error>([&] {
        builder.bind(fixture->selected, selected);
    });
    expect_throws<std::logic_error>([&] {
        builder.assign(fixture->name, "closed");
    });
    MapFixture stack_map_fixture;
    expect_throws<std::logic_error>([&] {
        builder.bind(stack_map_fixture.index, entries);
    });
    expect_throws<std::logic_error>([&] {
        static_cast<void>(std::move(builder).finish(root));
    });
}

void test_finish_rejects_invalid_roots() {
    RegionBuilder builder;
    expect_throws<std::invalid_argument>([&] {
        static_cast<void>(std::move(builder).finish(region_handle<int>{}));
    });

    RegionBuilder first;
    RegionBuilder second;
    const auto foreign_root = second.make_object<int>();
    expect_throws<std::invalid_argument>([&] {
        static_cast<void>(std::move(first).finish(foreign_root));
    });
}

} // namespace

int main() {
    test_checked_arithmetic();
    test_builder_and_view_gate();
    test_empty_and_capacity_boundaries();
    test_handle_and_destination_provenance();
    test_finish_closes_builder();
    test_finish_rejects_invalid_roots();
}
