#include "region.hpp"

#include <boost/typelayout.hpp>

#include <cstdint>
#include <type_traits>

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

int main() {}
