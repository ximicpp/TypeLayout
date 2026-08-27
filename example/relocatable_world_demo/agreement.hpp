// agreement.hpp -- Local TypeLayout Agreement for the world contract.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_AGREEMENT_HPP
#define BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_AGREEMENT_HPP

#include "world.hpp"

#include <boost/typelayout/tools/sig_types.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace relocatable_world_demo {

enum class agreement_result { match, differ, incomplete };

struct named_agreement {
    std::string_view key;
    bool matches;
};

namespace detail {

inline constexpr auto agreement_keys = [] {
    std::array<std::string_view, 4> keys{};
    std::size_t index = 0;
    for_each_contract_type([&]<typename T>(std::string_view key) {
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
    for (std::size_t i = 0; i < producer.type_count; ++i) {
        const auto& entry = producer.types[i];
        if (entry.name == nullptr || entry.layout_sig == nullptr) {
            return false;
        }

        const std::string_view entry_key{entry.name};
        bool canonical = false;
        for (std::size_t key_index = 0;
             key_index < agreement_keys.size(); ++key_index) {
            if (entry_key != agreement_keys[key_index]) {
                continue;
            }
            if (seen[key_index]) {
                return false;
            }
            seen[key_index] = true;
            canonical = true;
            break;
        }
        if (!canonical) {
            return false;
        }
    }

    for (const bool key_seen : seen) {
        if (!key_seen) {
            return false;
        }
    }
    return true;
}

template <typename T>
bool agreement_entry_matches(boost::typelayout::PlatformInfo producer,
                             std::string_view key) {
    if (producer.types == nullptr) {
        return false;
    }

    const boost::typelayout::TypeEntry* matching_entry = nullptr;
    for (std::size_t i = 0; i < producer.type_count; ++i) {
        const auto& entry = producer.types[i];
        if (entry.name == nullptr || std::string_view(entry.name) != key) {
            continue;
        }
        if (matching_entry != nullptr) {
            return false;
        }
        matching_entry = &entry;
    }

    if (matching_entry == nullptr || matching_entry->layout_sig == nullptr ||
        !matching_entry->byte_copy_safe) {
        return false;
    }

    constexpr auto current = boost::typelayout::get_layout_signature<T>();
    return std::string_view(matching_entry->layout_sig) ==
           std::string_view(current);
}

} // namespace detail

inline std::array<named_agreement, 4> current_agreement_details(
    boost::typelayout::PlatformInfo producer) {
    std::array<named_agreement, 4> details{};
    std::size_t index = 0;
    for_each_contract_type([&]<typename T>(std::string_view key) {
        details[index++] = {
            key,
            detail::agreement_entry_matches<T>(producer, key),
        };
    });
    return details;
}

inline agreement_result check_current_agreement(
    boost::typelayout::PlatformInfo producer) {
    if (!detail::has_complete_agreement_registry(producer)) {
        return agreement_result::incomplete;
    }

    for (const auto& entry : current_agreement_details(producer)) {
        if (!entry.matches) {
            return agreement_result::differ;
        }
    }
    return agreement_result::match;
}

} // namespace relocatable_world_demo

#endif // BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_AGREEMENT_HPP
