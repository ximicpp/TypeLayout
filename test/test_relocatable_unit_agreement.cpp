#include "agreement.hpp"
#include "sigs/unit_producer_ok.sig.hpp"
#include "sigs/unit_producer_packed.sig.hpp"

#include <boost/typelayout.hpp>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>

using namespace relocatable_unit_handoff_demo;

namespace {

struct NativePointerUnit {
    Effect* effect;
};

static_assert(!boost::typelayout::is_admitted_v<
    NativePointerUnit,
    boost::typelayout::TransferProfile::whole_region_relocation>);

void expect(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void expect_details(const std::array<named_agreement, 4>& details,
                    const std::array<bool, 4>& matches) {
    constexpr std::array<std::string_view, 4> keys{
        "UnitSnapshot", "Effect", "EffectRelativePtr", "AttributeEntry"};
    for (std::size_t index = 0; index < keys.size(); ++index) {
        expect(details[index].key == keys[index]);
        expect(details[index].matches == matches[index]);
    }
}

void test_normal_and_packed_agreement() {
    const auto normal =
        boost::typelayout::platform::unit_producer_ok::get_platform_info();
    const auto packed =
        boost::typelayout::platform::unit_producer_packed::get_platform_info();
    expect(check_current_unit_agreement(normal) == agreement_result::match);
    expect(check_current_unit_agreement(packed) == agreement_result::differ);
    expect_details(current_unit_agreement_details(normal),
                   {true, true, true, true});
    expect_details(current_unit_agreement_details(packed),
                   {true, false, true, true});

    std::array<boost::typelayout::TypeEntry, 4> permuted{
        normal.types[2], normal.types[0], normal.types[3], normal.types[1]};
    auto permuted_info = normal;
    permuted_info.types = permuted.data();
    expect(check_current_unit_agreement(permuted_info) ==
           agreement_result::match);
}

void test_incomplete_and_unsafe_evidence() {
    const auto normal =
        boost::typelayout::platform::unit_producer_ok::get_platform_info();

    auto short_info = normal;
    short_info.type_count = 3;
    expect(check_current_unit_agreement(short_info) ==
           agreement_result::incomplete);

    std::array<boost::typelayout::TypeEntry, 4> duplicate{
        normal.types[0], normal.types[0], normal.types[2], normal.types[3]};
    auto duplicate_info = normal;
    duplicate_info.types = duplicate.data();
    expect(check_current_unit_agreement(duplicate_info) ==
           agreement_result::incomplete);

    auto unknown = duplicate;
    unknown[1] = normal.types[1];
    unknown[1].name = "Unknown";
    auto unknown_info = normal;
    unknown_info.types = unknown.data();
    expect(check_current_unit_agreement(unknown_info) ==
           agreement_result::incomplete);

    auto missing_signature = duplicate;
    missing_signature[1] = normal.types[1];
    missing_signature[1].layout_sig = nullptr;
    auto missing_signature_info = normal;
    missing_signature_info.types = missing_signature.data();
    expect(check_current_unit_agreement(missing_signature_info) ==
           agreement_result::incomplete);

    auto unsafe = normal;
    auto unsafe_types = std::array<boost::typelayout::TypeEntry, 4>{
        normal.types[0], normal.types[1], normal.types[2], normal.types[3]};
    unsafe_types[2].byte_copy_safe = false;
    unsafe.types = unsafe_types.data();
    expect(check_current_unit_agreement(unsafe) == agreement_result::differ);
    expect_details(current_unit_agreement_details(unsafe),
                   {true, true, false, true});
}

void test_rejections_skip_loader() {
    const auto normal =
        boost::typelayout::platform::unit_producer_ok::get_platform_info();
    const auto packed =
        boost::typelayout::platform::unit_producer_packed::get_platform_info();
    int loader_calls = 0;
    if (check_current_unit_agreement(normal) == agreement_result::match) {
        ++loader_calls;
    }
    if (check_current_unit_agreement(packed) == agreement_result::match) {
        ++loader_calls;
    }
    if constexpr (boost::typelayout::is_admitted_v<
                      NativePointerUnit,
                      boost::typelayout::TransferProfile::whole_region_relocation>) {
        ++loader_calls;
    }
    expect(loader_calls == 1);
}

} // namespace

int main() {
    test_normal_and_packed_agreement();
    test_incomplete_and_unsafe_evidence();
    test_rejections_skip_loader();
}
