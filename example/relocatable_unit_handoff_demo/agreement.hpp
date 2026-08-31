// agreement.hpp -- Local TypeLayout Agreement for the unit contract.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_RELOCATABLE_UNIT_HANDOFF_DEMO_AGREEMENT_HPP
#define BOOST_TYPELAYOUT_RELOCATABLE_UNIT_HANDOFF_DEMO_AGREEMENT_HPP

#include "unit.hpp"

#include <boost/typelayout/tools/sig_types.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace relocatable_unit_handoff_demo {

enum class agreement_result { match, differ, incomplete };

struct named_agreement {
    std::string_view key;
    bool matches;
};

namespace detail {

inline constexpr auto agreement_keys = [] {
    std::array<std::string_view, 4> keys{};
    std::size_t index = 0;
    for_each_unit_contract_type([&]<typename T>(std::string_view key) {
        keys[index++] = key;
    });
    return keys;
}();

inline bool has_complete_agreement_registry(
    boost::typelayout::PlatformInfo producer) {
    if (producer.type_count != agreement_keys.size() ||
        producer.types == nullptr) {
        return false;
    }

    std::array<bool, agreement_keys.size()> seen{};
    for (std::size_t index = 0; index < producer.type_count; ++index) {
        const auto& entry = producer.types[index];
        if (entry.name == nullptr || entry.layout_sig == nullptr) {
            return false;
        }
        const std::string_view entry_key{entry.name};
        bool canonical = false;
        for (std::size_t key = 0; key < agreement_keys.size(); ++key) {
            if (entry_key != agreement_keys[key]) {
                continue;
            }
            if (seen[key]) {
                return false;
            }
            seen[key] = true;
            canonical = true;
            break;
        }
        if (!canonical) {
            return false;
        }
    }
    for (const bool present : seen) {
        if (!present) {
            return false;
        }
    }
    return true;
}

template <typename T>
bool agreement_entry_matches(boost::typelayout::PlatformInfo producer,
                             std::string_view key) {
    const boost::typelayout::TypeEntry* matching = nullptr;
    for (std::size_t index = 0; index < producer.type_count; ++index) {
        const auto& entry = producer.types[index];
        if (entry.name == nullptr || std::string_view(entry.name) != key) {
            continue;
        }
        if (matching != nullptr) {
            return false;
        }
        matching = &entry;
    }
    if (matching == nullptr || matching->layout_sig == nullptr ||
        !matching->byte_copy_safe) {
        return false;
    }
    constexpr auto current = boost::typelayout::get_layout_signature<T>();
    return std::string_view(matching->layout_sig) == std::string_view(current);
}

} // namespace detail

inline std::array<named_agreement, 4> current_unit_agreement_details(
    boost::typelayout::PlatformInfo producer) {
    std::array<named_agreement, 4> details{};
    std::size_t index = 0;
    for_each_unit_contract_type([&]<typename T>(std::string_view key) {
        details[index++] = {
            key,
            detail::agreement_entry_matches<T>(producer, key),
        };
    });
    return details;
}

inline agreement_result check_current_unit_agreement(
    boost::typelayout::PlatformInfo producer) {
    if (!detail::has_complete_agreement_registry(producer)) {
        return agreement_result::incomplete;
    }
    for (const auto& detail : current_unit_agreement_details(producer)) {
        if (!detail.matches) {
            return agreement_result::differ;
        }
    }
    return agreement_result::match;
}

} // namespace relocatable_unit_handoff_demo

#endif // BOOST_TYPELAYOUT_RELOCATABLE_UNIT_HANDOFF_DEMO_AGREEMENT_HPP
