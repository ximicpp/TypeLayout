#include "world.hpp"

#include "sigs/producer_ok.sig.hpp"
#include "sigs/producer_packed.sig.hpp"

#include <cstdio>
#include <string_view>

enum class AgreementResult { match, differ, incomplete };

AgreementResult check_agreement(
    boost::typelayout::PlatformInfo producer);

const boost::typelayout::TypeEntry* find_fixture_entry(
    boost::typelayout::PlatformInfo producer,
    std::string_view key) {
    for (std::size_t i = 0; i < producer.type_count; ++i) {
        if (std::string_view(producer.types[i].name) == key) {
            return &producer.types[i];
        }
    }
    return nullptr;
}

template <typename T>
bool entry_matches(const boost::typelayout::TypeEntry& entry) {
    constexpr auto current = boost::typelayout::get_layout_signature<T>();
    return entry.byte_copy_safe &&
        std::string_view(entry.layout_sig) == std::string_view(current);
}

template <typename T>
bool fixture_entry_matches(
    boost::typelayout::PlatformInfo producer,
    std::string_view key) {
    const auto* entry = find_fixture_entry(producer, key);
    return entry != nullptr && entry_matches<T>(*entry);
}

AgreementResult check_agreement(
    boost::typelayout::PlatformInfo producer) {
    bool saw_missing = false;
    bool saw_difference = false;

    xoffset_world_demo::for_each_contract_type(
        [&]<typename T>(std::string_view key) {
            const auto* entry = find_fixture_entry(producer, key);
            if (entry == nullptr) {
                saw_missing = true;
            } else if (!entry_matches<T>(*entry)) {
                saw_difference = true;
            }
        });

    if (saw_missing) {
        return AgreementResult::incomplete;
    }
    return saw_difference ? AgreementResult::differ : AgreementResult::match;
}

int main() {
    static_assert(xoffset_world_demo::world_contract_admitted_v);

    const auto ok = check_agreement(
        boost::typelayout::platform::producer_ok::get_platform_info());
    const auto packed = check_agreement(
        boost::typelayout::platform::producer_packed::get_platform_info());
    if (ok != AgreementResult::match ||
        packed != AgreementResult::differ) {
        return 1;
    }

    const auto packed_info =
        boost::typelayout::platform::producer_packed::get_platform_info();
    if (!fixture_entry_matches<xoffset_world_demo::WorldSnapshot>(
            packed_info, "WorldSnapshot") ||
        fixture_entry_matches<xoffset_world_demo::Entity>(
            packed_info, "Entity") ||
        !fixture_entry_matches<xoffset_world_demo::EntityRelativePtr>(
            packed_info, "EntityRelativePtr") ||
        !fixture_entry_matches<xoffset_world_demo::EntityIndexEntry>(
            packed_info, "EntityIndexEntry")) {
        return 1;
    }

    xoffset_world_demo::relative_ptr<xoffset_world_demo::Entity> null_entity;
    const auto& const_null_entity = null_entity;
    if (null_entity.get() != nullptr || const_null_entity.get() != nullptr) {
        std::fprintf(stderr, "relative_ptr null resolution failed\n");
        return 1;
    }

    std::printf("Admission[whole_region_relocation]: PASS\n");
    std::printf("Agreement[producer_ok, 4 types]: MATCH\n");
    std::printf("Negative[producer packing ABI drift]: Agreement DIFFER, load skipped\n");
    return 0;
}
