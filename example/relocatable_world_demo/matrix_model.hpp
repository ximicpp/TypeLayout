// matrix_model.hpp -- Reflection-independent relocatable-world matrix model.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_MATRIX_MODEL_HPP
#define BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_MATRIX_MODEL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace relocatable_world_demo::matrix {

enum class profile_id : std::uint8_t {
    authoritative,
    local_arm64_macos,
};

enum class node_id : std::uint8_t {
    x86_64_linux_gcc,
    x86_64_linux_clang,
    arm64_linux_gcc,
    arm64_linux_clang,
    arm64_macos_clang,
    x86_64_macos_clang,
};

enum class key_id : std::uint8_t {
    world_snapshot,
    entity,
    entity_relative_ptr,
    entity_index_entry,
};

enum class agreement_status : std::uint8_t {
    permit,
    reject,
    incomplete,
};

enum class transfer_status : std::uint8_t {
    pass,
    skipped_typelayout_reject,
    reject_envelope,
    reject_region,
    reject_graph,
    incomplete,
};

enum class closure_status : std::uint8_t {
    pass,
    reject,
    incomplete,
};

inline constexpr std::size_t key_count = 4;

inline constexpr std::array<node_id, 6> authoritative_node_ids = {
    node_id::x86_64_linux_gcc,
    node_id::x86_64_linux_clang,
    node_id::arm64_linux_gcc,
    node_id::arm64_linux_clang,
    node_id::arm64_macos_clang,
    node_id::x86_64_macos_clang,
};

inline constexpr std::array<node_id, 5> local_node_ids = {
    node_id::x86_64_linux_gcc,
    node_id::x86_64_linux_clang,
    node_id::arm64_linux_gcc,
    node_id::arm64_linux_clang,
    node_id::arm64_macos_clang,
};

inline constexpr std::array<key_id, key_count> key_ids = {
    key_id::world_snapshot,
    key_id::entity,
    key_id::entity_relative_ptr,
    key_id::entity_index_entry,
};

inline constexpr std::array<std::string_view, 6> node_names = {
    "x86_64_linux_gcc",
    "x86_64_linux_clang",
    "arm64_linux_gcc",
    "arm64_linux_clang",
    "arm64_macos_clang",
    "x86_64_macos_clang",
};

inline constexpr std::array<std::string_view, key_count> key_names = {
    "WorldSnapshot",
    "Entity",
    "EntityRelativePtr",
    "EntityIndexEntry",
};

constexpr std::string_view name(node_id value) noexcept {
    const auto index = static_cast<std::size_t>(value);
    return index < node_names.size() ? node_names[index] : std::string_view{};
}

constexpr std::string_view name(key_id value) noexcept {
    const auto index = static_cast<std::size_t>(value);
    return index < key_names.size() ? key_names[index] : std::string_view{};
}

constexpr std::string_view name(agreement_status value) noexcept {
    switch (value) {
    case agreement_status::permit:
        return "PERMIT";
    case agreement_status::reject:
        return "REJECT";
    case agreement_status::incomplete:
        return "INCOMPLETE";
    }
    return {};
}

constexpr std::string_view name(transfer_status value) noexcept {
    switch (value) {
    case transfer_status::pass:
        return "PASS";
    case transfer_status::skipped_typelayout_reject:
        return "SKIPPED_TYPELAYOUT_REJECT";
    case transfer_status::reject_envelope:
        return "REJECT_ENVELOPE";
    case transfer_status::reject_region:
        return "REJECT_REGION";
    case transfer_status::reject_graph:
        return "REJECT_GRAPH";
    case transfer_status::incomplete:
        return "INCOMPLETE";
    }
    return {};
}

constexpr std::string_view name(closure_status value) noexcept {
    switch (value) {
    case closure_status::pass:
        return "PASS";
    case closure_status::reject:
        return "REJECT";
    case closure_status::incomplete:
        return "INCOMPLETE";
    }
    return {};
}

constexpr std::span<const node_id> profile_nodes(profile_id profile) noexcept {
    if (profile == profile_id::authoritative) {
        return authoritative_node_ids;
    }
    return local_node_ids;
}

constexpr std::size_t expected_pair_count(profile_id profile) noexcept {
    const auto count = profile_nodes(profile).size();
    return count * (count - 1) / 2;
}

constexpr std::size_t expected_transfer_count(profile_id profile) noexcept {
    const auto count = profile_nodes(profile).size();
    return count * (count - 1);
}

struct run_identity {
    std::string_view source_sha;
    std::string_view workflow_run;
    std::string_view sources_sha256;
    std::string_view outputs_sha256;
};

constexpr bool operator==(const run_identity& left,
                          const run_identity& right) noexcept {
    return left.source_sha == right.source_sha &&
        left.workflow_run == right.workflow_run &&
        left.sources_sha256 == right.sources_sha256 &&
        left.outputs_sha256 == right.outputs_sha256;
}

struct producer_record {
    node_id node{};
    bool present{};
    std::string_view error;
    std::string_view provenance_sha256;
    run_identity run{};
    bool authoritative_eligible{};
    std::array<bool, key_count> admission{};
    std::array<std::string_view, key_count> signatures{};
    bool region_present{};
    std::string_view region_filename;
    std::string_view region_sha256;
};

struct provenance_binding {
    node_id node{};
    bool present{};
    std::string_view sha256;
};

struct named_decision {
    key_id key{};
    agreement_status status{agreement_status::incomplete};
};

struct pair_record {
    node_id left{};
    node_id right{};
    std::array<named_decision, key_count> decisions{};
};

struct consumer_record {
    node_id consumer{};
    bool present{};
    run_identity run{};
    bool authoritative_eligible{};
};

struct consumer_build_record {
    run_identity run{};
    std::string_view execution;
    std::string_view runner;
    std::string_view runner_image;
    std::string_view toolchain_artifact_sha256;
    std::string_view compiler_family;
    std::string_view compiler_revision;
    std::string_view compiler_version;
    std::string_view target;
    std::string_view stdlib;
    std::string_view flags;
    std::string_view xcode_version;
    std::string_view xcode_build;
    std::string_view sdk_version;
    std::string_view sdk_build;
    std::string_view deployment_target;
    bool sdk_locked{};
    bool authoritative_eligible{};
};

struct transfer_record {
    node_id consumer{};
    node_id producer{};
    transfer_status status{transfer_status::incomplete};
};

struct matrix_view {
    profile_id profile{};
    run_identity expected_run{};
    std::span<const producer_record> producers;
    std::span<const provenance_binding> agreement_provenance;
    std::span<const pair_record> agreements;
    std::span<const consumer_record> consumers;
    std::span<const transfer_record> transfers;
};

struct closure_counts {
    std::size_t nodes{};
    std::size_t pairs{};
    std::size_t named_decisions{};
    std::size_t named_permits{};
    std::size_t consumers{};
    std::size_t transfers{};
    std::size_t passes{};
};

struct closure_result {
    closure_status status{closure_status::incomplete};
    bool authoritative{};
    closure_counts counts{};
};

constexpr bool profile_contains(profile_id profile, node_id node) noexcept {
    for (const auto expected : profile_nodes(profile)) {
        if (expected == node) {
            return true;
        }
    }
    return false;
}

constexpr const producer_record* unique_producer(
    std::span<const producer_record> producers,
    node_id node) noexcept {
    const producer_record* result = nullptr;
    for (const auto& producer : producers) {
        if (producer.node != node) {
            continue;
        }
        if (result != nullptr) {
            return nullptr;
        }
        result = &producer;
    }
    return result;
}

constexpr agreement_status compute_agreement(
    std::span<const producer_record> producers,
    node_id left,
    node_id right,
    key_id key) noexcept {
    const auto key_index = static_cast<std::size_t>(key);
    const auto* left_record = unique_producer(producers, left);
    const auto* right_record = unique_producer(producers, right);
    if (key_index >= key_count || left_record == nullptr ||
        right_record == nullptr || !left_record->present ||
        !right_record->present || left_record->signatures[key_index].empty() ||
        right_record->signatures[key_index].empty()) {
        return agreement_status::incomplete;
    }
    if (!left_record->admission[key_index] ||
        !right_record->admission[key_index] ||
        left_record->signatures[key_index] !=
            right_record->signatures[key_index]) {
        return agreement_status::reject;
    }
    return agreement_status::permit;
}

template <typename Loader>
constexpr transfer_status load_after_typelayout_gate(
    const std::array<bool, key_count>& consumer_admission,
    const std::array<std::string_view, key_count>& consumer_signatures,
    const producer_record& producer,
    Loader&& loader) {
    if (!producer.present) {
        return transfer_status::incomplete;
    }
    for (std::size_t key = 0; key < key_count; ++key) {
        if (!consumer_admission[key] || !producer.admission[key] ||
            consumer_signatures[key].empty() ||
            producer.signatures[key].empty() ||
            consumer_signatures[key] != producer.signatures[key]) {
            return transfer_status::skipped_typelayout_reject;
        }
    }
    return loader();
}

namespace detail {

template <typename Record, typename Projection>
constexpr std::size_t count_identity(std::span<const Record> records,
                                     Projection projection,
                                     node_id expected) noexcept {
    std::size_t count = 0;
    for (const auto& record : records) {
        if (projection(record) == expected) {
            ++count;
        }
    }
    return count;
}

constexpr const pair_record* unique_pair(std::span<const pair_record> pairs,
                                         node_id left,
                                         node_id right) noexcept {
    const pair_record* result = nullptr;
    for (const auto& pair : pairs) {
        if (pair.left != left || pair.right != right) {
            continue;
        }
        if (result != nullptr) {
            return nullptr;
        }
        result = &pair;
    }
    return result;
}

constexpr const transfer_record* unique_transfer(
    std::span<const transfer_record> transfers,
    node_id consumer,
    node_id producer) noexcept {
    const transfer_record* result = nullptr;
    for (const auto& transfer : transfers) {
        if (transfer.consumer != consumer || transfer.producer != producer) {
            continue;
        }
        if (result != nullptr) {
            return nullptr;
        }
        result = &transfer;
    }
    return result;
}

constexpr const provenance_binding* unique_binding(
    std::span<const provenance_binding> bindings,
    node_id node) noexcept {
    const provenance_binding* result = nullptr;
    for (const auto& binding : bindings) {
        if (binding.node != node) {
            continue;
        }
        if (result != nullptr) {
            return nullptr;
        }
        result = &binding;
    }
    return result;
}

constexpr bool valid_decision_keys(const pair_record& pair) noexcept {
    for (const auto expected : key_ids) {
        std::size_t count = 0;
        for (const auto& decision : pair.decisions) {
            if (decision.key == expected) {
                ++count;
            }
        }
        if (count != 1) {
            return false;
        }
    }
    return true;
}

constexpr const named_decision* decision_for(const pair_record& pair,
                                             key_id key) noexcept {
    const named_decision* result = nullptr;
    for (const auto& decision : pair.decisions) {
        if (decision.key != key) {
            continue;
        }
        if (result != nullptr) {
            return nullptr;
        }
        result = &decision;
    }
    return result;
}

} // namespace detail

constexpr closure_result close_matrix(const matrix_view& input) noexcept {
    closure_result result{};
    bool incomplete = false;
    bool rejected = false;
    bool authoritative_eligible =
        input.profile == profile_id::authoritative;
    const auto nodes = profile_nodes(input.profile);

    if (input.producers.size() != nodes.size() ||
        input.agreement_provenance.size() != nodes.size() ||
        input.agreements.size() != expected_pair_count(input.profile) ||
        input.consumers.size() != nodes.size() ||
        input.transfers.size() != expected_transfer_count(input.profile)) {
        incomplete = true;
    }

    for (const auto& producer : input.producers) {
        if (!profile_contains(input.profile, producer.node)) {
            incomplete = true;
        }
    }
    for (const auto& consumer : input.consumers) {
        if (!profile_contains(input.profile, consumer.consumer)) {
            incomplete = true;
        }
    }
    for (const auto& pair : input.agreements) {
        if (!profile_contains(input.profile, pair.left) ||
            !profile_contains(input.profile, pair.right) ||
            pair.left == pair.right) {
            incomplete = true;
        }
    }
    for (const auto& transfer : input.transfers) {
        if (!profile_contains(input.profile, transfer.consumer) ||
            !profile_contains(input.profile, transfer.producer) ||
            transfer.consumer == transfer.producer) {
            incomplete = true;
        }
    }

    for (const auto node : nodes) {
        const auto producer_occurrences = detail::count_identity(
            input.producers,
            [](const producer_record& record) { return record.node; },
            node);
        const auto* producer = unique_producer(input.producers, node);
        if (producer_occurrences != 1 || producer == nullptr ||
            !producer->present) {
            incomplete = true;
        } else {
            ++result.counts.nodes;
            if (!(producer->run == input.expected_run)) {
                incomplete = true;
            }
            authoritative_eligible = authoritative_eligible &&
                producer->authoritative_eligible;
        }

        const auto* binding = detail::unique_binding(
            input.agreement_provenance, node);
        const auto binding_occurrences = detail::count_identity(
            input.agreement_provenance,
            [](const provenance_binding& record) { return record.node; },
            node);
        if (binding_occurrences != 1 || binding == nullptr ||
            producer == nullptr || binding->present != producer->present ||
            (binding->present &&
             binding->sha256 != producer->provenance_sha256)) {
            incomplete = true;
        }

        const auto consumer_occurrences = detail::count_identity(
            input.consumers,
            [](const consumer_record& record) { return record.consumer; },
            node);
        const consumer_record* consumer = nullptr;
        for (const auto& candidate : input.consumers) {
            if (candidate.consumer == node) {
                consumer = consumer == nullptr ? &candidate : nullptr;
                if (consumer == nullptr) {
                    break;
                }
            }
        }
        if (consumer_occurrences != 1 || consumer == nullptr ||
            !consumer->present) {
            incomplete = true;
        } else {
            ++result.counts.consumers;
            if (!(consumer->run == input.expected_run)) {
                incomplete = true;
            }
            authoritative_eligible = authoritative_eligible &&
                consumer->authoritative_eligible;
        }
    }

    for (std::size_t left_index = 0; left_index < nodes.size(); ++left_index) {
        for (std::size_t right_index = left_index + 1;
             right_index < nodes.size(); ++right_index) {
            const auto left = nodes[left_index];
            const auto right = nodes[right_index];
            const auto* pair = detail::unique_pair(input.agreements, left, right);
            std::size_t occurrences = 0;
            for (const auto& candidate : input.agreements) {
                if (candidate.left == left && candidate.right == right) {
                    ++occurrences;
                }
            }
            if (occurrences != 1 || pair == nullptr ||
                !detail::valid_decision_keys(*pair)) {
                incomplete = true;
                continue;
            }
            ++result.counts.pairs;
            for (const auto key : key_ids) {
                const auto* decision = detail::decision_for(*pair, key);
                if (decision == nullptr) {
                    incomplete = true;
                    continue;
                }
                ++result.counts.named_decisions;
                if (decision->status == agreement_status::permit) {
                    ++result.counts.named_permits;
                } else if (decision->status == agreement_status::incomplete) {
                    incomplete = true;
                } else {
                    rejected = true;
                }
                if (decision->status != compute_agreement(
                        input.producers, left, right, key)) {
                    incomplete = true;
                }
            }
        }
    }

    for (const auto consumer : nodes) {
        for (const auto producer : nodes) {
            if (consumer == producer) {
                continue;
            }
            const auto* transfer = detail::unique_transfer(
                input.transfers, consumer, producer);
            std::size_t occurrences = 0;
            for (const auto& candidate : input.transfers) {
                if (candidate.consumer == consumer &&
                    candidate.producer == producer) {
                    ++occurrences;
                }
            }
            if (occurrences != 1 || transfer == nullptr) {
                incomplete = true;
                continue;
            }
            ++result.counts.transfers;
            if (transfer->status == transfer_status::pass) {
                ++result.counts.passes;
            } else if (transfer->status == transfer_status::incomplete) {
                incomplete = true;
            } else {
                rejected = true;
            }
        }
    }

    result.authoritative = !incomplete && authoritative_eligible;
    if (incomplete) {
        result.status = closure_status::incomplete;
    } else if (rejected) {
        result.status = closure_status::reject;
    } else {
        result.status = closure_status::pass;
    }
    return result;
}

} // namespace relocatable_world_demo::matrix

#endif // BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_MATRIX_MODEL_HPP
