// matrix_check.cpp -- C++20-only relocatable-world closure writer.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "evidence_json.hpp"
#include "matrix_model.hpp"
#include "relocatable_world_matrix_input.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

namespace fs = std::filesystem;
namespace generated = relocatable_world_demo::generated::matrix_input;
namespace matrix = relocatable_world_demo::matrix;

using relocatable_world_demo::evidence_json::write_key;
using relocatable_world_demo::evidence_json::write_string;

class atomic_output {
public:
    explicit atomic_output(fs::path destination)
        : destination_(std::move(destination)),
          temporary_(destination_.string() + ".tmp") {
        std::error_code ignored;
        fs::remove(temporary_, ignored);
    }

    ~atomic_output() {
        if (!committed_) {
            std::error_code ignored;
            fs::remove(temporary_, ignored);
        }
    }

    const fs::path& temporary() const noexcept { return temporary_; }

    void commit() {
        fs::rename(temporary_, destination_);
        committed_ = true;
    }

private:
    fs::path destination_;
    fs::path temporary_;
    bool committed_ = false;
};

constexpr std::string_view profile_name(matrix::profile_id profile) noexcept {
    return profile == matrix::profile_id::authoritative
        ? "authoritative"
        : "local-arm64-macos";
}

std::size_t producer_occurrences(const matrix::matrix_view& input,
                                 matrix::node_id node) {
    std::size_t count = 0;
    for (const auto& record : input.producers) {
        count += record.node == node ? 1U : 0U;
    }
    return count;
}

std::size_t consumer_occurrences(const matrix::matrix_view& input,
                                 matrix::node_id node) {
    std::size_t count = 0;
    for (const auto& record : input.consumers) {
        count += record.consumer == node ? 1U : 0U;
    }
    return count;
}

std::size_t pair_occurrences(const matrix::matrix_view& input,
                             matrix::node_id left,
                             matrix::node_id right) {
    std::size_t count = 0;
    for (const auto& record : input.agreements) {
        count += record.left == left && record.right == right ? 1U : 0U;
    }
    return count;
}

std::size_t transfer_occurrences(const matrix::matrix_view& input,
                                 matrix::node_id consumer,
                                 matrix::node_id producer) {
    std::size_t count = 0;
    for (const auto& record : input.transfers) {
        count += record.consumer == consumer && record.producer == producer
            ? 1U
            : 0U;
    }
    return count;
}

std::size_t decision_occurrences(const matrix::matrix_view& input,
                                 matrix::node_id left,
                                 matrix::node_id right,
                                 matrix::key_id key) {
    std::size_t count = 0;
    for (const auto& pair : input.agreements) {
        if (pair.left != left || pair.right != right) {
            continue;
        }
        for (const auto& decision : pair.decisions) {
            count += decision.key == key ? 1U : 0U;
        }
    }
    return count;
}

void write_pair_identity(std::ostream& output,
                         matrix::node_id left,
                         matrix::node_id right) {
    output << "{";
    write_key(output, "left");
    write_string(output, matrix::name(left));
    output << ", ";
    write_key(output, "right");
    write_string(output, matrix::name(right));
    output << "}";
}

void write_decision_identity(std::ostream& output,
                             matrix::node_id left,
                             matrix::node_id right,
                             matrix::key_id key) {
    output << "{";
    write_key(output, "left");
    write_string(output, matrix::name(left));
    output << ", ";
    write_key(output, "right");
    write_string(output, matrix::name(right));
    output << ", ";
    write_key(output, "key");
    write_string(output, matrix::name(key));
    output << "}";
}

void write_transfer_identity(std::ostream& output,
                             matrix::node_id consumer,
                             matrix::node_id producer) {
    output << "{";
    write_key(output, "consumer");
    write_string(output, matrix::name(consumer));
    output << ", ";
    write_key(output, "producer");
    write_string(output, matrix::name(producer));
    output << "}";
}

void write_expected(std::ostream& output, matrix::profile_id profile) {
    const auto nodes = matrix::profile_nodes(profile);
    output << "{\n      ";
    write_key(output, "nodes");
    output << "[";
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        write_string(output, matrix::name(nodes[index]));
        output << (index + 1 == nodes.size() ? "" : ", ");
    }
    output << "],\n      ";
    write_key(output, "pairs");
    output << "[";
    std::size_t pair_index = 0;
    for (std::size_t left = 0; left < nodes.size(); ++left) {
        for (std::size_t right = left + 1; right < nodes.size(); ++right) {
            if (pair_index++ != 0) {
                output << ", ";
            }
            write_pair_identity(output, nodes[left], nodes[right]);
        }
    }
    output << "],\n      ";
    write_key(output, "named_decisions");
    output << "[";
    std::size_t decision_index = 0;
    for (std::size_t left = 0; left < nodes.size(); ++left) {
        for (std::size_t right = left + 1; right < nodes.size(); ++right) {
            for (const auto key : matrix::key_ids) {
                if (decision_index++ != 0) {
                    output << ", ";
                }
                write_decision_identity(output, nodes[left], nodes[right], key);
            }
        }
    }
    output << "],\n      ";
    write_key(output, "consumers");
    output << "[";
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        write_string(output, matrix::name(nodes[index]));
        output << (index + 1 == nodes.size() ? "" : ", ");
    }
    output << "],\n      ";
    write_key(output, "transfers");
    output << "[";
    std::size_t transfer_index = 0;
    for (const auto consumer : nodes) {
        for (const auto producer : nodes) {
            if (consumer == producer) {
                continue;
            }
            if (transfer_index++ != 0) {
                output << ", ";
            }
            write_transfer_identity(output, consumer, producer);
        }
    }
    output << "]\n    }";
}

void write_identity_diagnostics(std::ostream& output,
                                const matrix::matrix_view& input,
                                bool duplicates) {
    const auto nodes = matrix::profile_nodes(input.profile);
    const auto selected = [duplicates](std::size_t occurrences,
                                       bool present = true) {
        return duplicates ? occurrences > 1 : occurrences == 0 || !present;
    };
    output << "{\n      ";
    write_key(output, "nodes");
    output << "[";
    bool first = true;
    for (const auto node : nodes) {
        const auto count = producer_occurrences(input, node);
        const auto* producer = matrix::unique_producer(input.producers, node);
        if (selected(count, producer != nullptr && producer->present)) {
            if (!first) {
                output << ", ";
            }
            first = false;
            write_string(output, matrix::name(node));
        }
    }
    output << "],\n      ";
    write_key(output, "pairs");
    output << "[";
    first = true;
    for (std::size_t left = 0; left < nodes.size(); ++left) {
        for (std::size_t right = left + 1; right < nodes.size(); ++right) {
            if (!selected(pair_occurrences(input, nodes[left], nodes[right]))) {
                continue;
            }
            if (!first) {
                output << ", ";
            }
            first = false;
            write_pair_identity(output, nodes[left], nodes[right]);
        }
    }
    output << "],\n      ";
    write_key(output, "named_decisions");
    output << "[";
    first = true;
    for (std::size_t left = 0; left < nodes.size(); ++left) {
        for (std::size_t right = left + 1; right < nodes.size(); ++right) {
            for (const auto key : matrix::key_ids) {
                if (!selected(decision_occurrences(
                        input, nodes[left], nodes[right], key))) {
                    continue;
                }
                if (!first) {
                    output << ", ";
                }
                first = false;
                write_decision_identity(output, nodes[left], nodes[right], key);
            }
        }
    }
    output << "],\n      ";
    write_key(output, "consumers");
    output << "[";
    first = true;
    for (const auto node : nodes) {
        const auto count = consumer_occurrences(input, node);
        const matrix::consumer_record* consumer = nullptr;
        if (count == 1) {
            for (const auto& candidate : input.consumers) {
                if (candidate.consumer == node) {
                    consumer = &candidate;
                    break;
                }
            }
        }
        if (selected(count, consumer != nullptr && consumer->present)) {
            if (!first) {
                output << ", ";
            }
            first = false;
            write_string(output, matrix::name(node));
        }
    }
    output << "],\n      ";
    write_key(output, "transfers");
    output << "[";
    first = true;
    for (const auto consumer : nodes) {
        for (const auto producer : nodes) {
            if (consumer == producer ||
                !selected(transfer_occurrences(input, consumer, producer))) {
                continue;
            }
            if (!first) {
                output << ", ";
            }
            first = false;
            write_transfer_identity(output, consumer, producer);
        }
    }
    output << "]\n    }";
}

void write_closure(std::ostream& output,
                   const matrix::matrix_view& input,
                   std::string_view agreements_sha256) {
    const auto result = matrix::close_matrix(input);
    output << "{\n  ";
    write_key(output, "schema");
    output << "1,\n  ";
    write_key(output, "profile");
    write_string(output, profile_name(input.profile));
    output << ",\n  ";
    write_key(output, "authoritative");
    output << (result.authoritative ? "true" : "false") << ",\n  ";
    write_key(output, "run");
    output << "{\n    ";
    write_key(output, "source_sha");
    write_string(output, input.expected_run.source_sha);
    output << ",\n    ";
    write_key(output, "workflow_run");
    write_string(output, input.expected_run.workflow_run);
    output << ",\n    ";
    write_key(output, "sources_sha256");
    write_string(output, input.expected_run.sources_sha256);
    output << ",\n    ";
    write_key(output, "outputs_sha256");
    write_string(output, input.expected_run.outputs_sha256);
    output << "\n  },\n  ";
    write_key(output, "agreements_sha256");
    write_string(output, agreements_sha256);
    output << ",\n  ";
    write_key(output, "expected");
    write_expected(output, input.profile);
    output << ",\n  ";
    write_key(output, "counts");
    output << "{\n    ";
    write_key(output, "nodes");
    output << result.counts.nodes << ",\n    ";
    write_key(output, "pairs");
    output << result.counts.pairs << ",\n    ";
    write_key(output, "named_decisions");
    output << result.counts.named_decisions << ",\n    ";
    write_key(output, "named_permits");
    output << result.counts.named_permits << ",\n    ";
    write_key(output, "consumers");
    output << result.counts.consumers << ",\n    ";
    write_key(output, "transfers");
    output << result.counts.transfers << ",\n    ";
    write_key(output, "passes");
    output << result.counts.passes << "\n  },\n  ";
    write_key(output, "missing");
    write_identity_diagnostics(output, input, false);
    output << ",\n  ";
    write_key(output, "duplicates");
    write_identity_diagnostics(output, input, true);
    output << ",\n  ";
    write_key(output, "status");
    write_string(output, matrix::name(result.status));
    output << ",\n  ";
    write_key(output, "error");
    output << "null\n}\n";
}

struct self_test_fixture {
    matrix::profile_id profile{};
    matrix::run_identity run{
        "1111111111111111111111111111111111111111", "123.1",
        "2222222222222222222222222222222222222222222222222222222222222222",
        "3333333333333333333333333333333333333333333333333333333333333333"};
    std::array<matrix::producer_record, 6> producers{};
    std::array<matrix::provenance_binding, 6> bindings{};
    std::array<matrix::pair_record, 15> agreements{};
    std::array<matrix::consumer_record, 6> consumers{};
    std::array<matrix::transfer_record, 30> transfers{};
    std::size_t nodes{};
    std::size_t pairs{};
    std::size_t edges{};

    matrix::matrix_view view() const {
        return {profile,
                run,
                {producers.data(), nodes},
                {bindings.data(), nodes},
                {agreements.data(), pairs},
                {consumers.data(), nodes},
                {transfers.data(), edges}};
    }
};

self_test_fixture make_self_test_fixture(matrix::profile_id profile) {
    self_test_fixture result{};
    result.profile = profile;
    const auto nodes = matrix::profile_nodes(profile);
    result.nodes = nodes.size();
    result.pairs = matrix::expected_pair_count(profile);
    result.edges = matrix::expected_transfer_count(profile);
    constexpr std::array<std::string_view, matrix::key_count> signatures{
        "world", "entity", "relative", "index"};
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        result.producers[index] = {
            nodes[index], true, {},
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            result.run, true, {true, true, true, true}, signatures, true,
            "node.region",
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"};
        result.bindings[index] = {
            nodes[index], true, result.producers[index].provenance_sha256};
        result.consumers[index] = {nodes[index], true, result.run, true};
    }
    std::size_t pair_index = 0;
    for (std::size_t left = 0; left < nodes.size(); ++left) {
        for (std::size_t right = left + 1; right < nodes.size(); ++right) {
            auto& pair = result.agreements[pair_index++];
            pair.left = nodes[left];
            pair.right = nodes[right];
            for (std::size_t key = 0; key < matrix::key_count; ++key) {
                pair.decisions[key] = {
                    static_cast<matrix::key_id>(key),
                    matrix::agreement_status::permit};
            }
        }
    }
    std::size_t edge = 0;
    for (const auto consumer : nodes) {
        for (const auto producer : nodes) {
            if (consumer != producer) {
                result.transfers[edge++] = {
                    consumer, producer, matrix::transfer_status::pass};
            }
        }
    }
    return result;
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void run_self_tests() {
    const auto complete = make_self_test_fixture(
        matrix::profile_id::authoritative);
    const auto complete_result = matrix::close_matrix(complete.view());
    require(complete_result.status == matrix::closure_status::pass &&
                complete_result.authoritative,
            "complete authoritative fixture did not pass");

    auto missing = complete;
    missing.producers[0].present = false;
    require(matrix::close_matrix(missing.view()).status ==
                matrix::closure_status::incomplete,
            "missing node did not make closure incomplete");

    auto rejected = complete;
    rejected.producers[0].admission[0] = false;
    for (std::size_t index = 0; index < rejected.pairs; ++index) {
        if (rejected.agreements[index].left == rejected.producers[0].node ||
            rejected.agreements[index].right == rejected.producers[0].node) {
            rejected.agreements[index].decisions[0].status =
                matrix::agreement_status::reject;
        }
    }
    require(matrix::close_matrix(rejected.view()).status ==
                matrix::closure_status::reject,
            "Agreement rejection did not reject closure");

    auto reject_and_missing = rejected;
    reject_and_missing.consumers[0].present = false;
    require(matrix::close_matrix(reject_and_missing.view()).status ==
                matrix::closure_status::incomplete,
            "incomplete did not take precedence over reject");

    auto duplicate_node = complete;
    duplicate_node.producers[0].node = duplicate_node.producers[1].node;
    require(matrix::close_matrix(duplicate_node.view()).status ==
                matrix::closure_status::incomplete,
            "duplicate node was accepted");

    auto duplicate_pair = complete;
    duplicate_pair.agreements[0] = duplicate_pair.agreements[1];
    require(matrix::close_matrix(duplicate_pair.view()).status ==
                matrix::closure_status::incomplete,
            "duplicate pair was accepted");

    auto duplicate_edge = complete;
    duplicate_edge.transfers[0] = duplicate_edge.transfers[1];
    require(matrix::close_matrix(duplicate_edge.view()).status ==
                matrix::closure_status::incomplete,
            "duplicate directed edge was accepted");

    auto self_edge = complete;
    self_edge.transfers[0].producer = self_edge.transfers[0].consumer;
    require(matrix::close_matrix(self_edge.view()).status ==
                matrix::closure_status::incomplete,
            "self edge was accepted");

    const auto local = make_self_test_fixture(
        matrix::profile_id::local_arm64_macos);
    const auto local_result = matrix::close_matrix(local.view());
    require(local_result.status == matrix::closure_status::pass &&
                !local_result.authoritative,
            "complete local fixture did not pass non-authoritatively");

    auto mismatched_producer = complete.producers[1];
    mismatched_producer.signatures[0] = "different signature";
    int corrupt_region_loader_calls = 0;
    const auto gate_status = matrix::load_after_typelayout_gate(
        complete.producers[0].admission,
        complete.producers[0].signatures,
        mismatched_producer,
        [&] {
            ++corrupt_region_loader_calls;
            return matrix::transfer_status::reject_region;
        });
    require(
        gate_status == matrix::transfer_status::skipped_typelayout_reject &&
            corrupt_region_loader_calls == 0,
        "signature mismatch reached corrupt region loader");

    std::cout << "SELFTEST PASS: nodes=6 agreements=15 named=60 transfers=30\n"
                 "SELFTEST PASS: local_nodes=5 agreements=10 named=40 "
                 "transfers=20 authoritative=false\n"
                 "SELFTEST PASS: incomplete/reject/pass precedence\n";
}

matrix::matrix_view generated_view() {
    return {generated::profile,
            generated::expected_run,
            generated::producers,
            generated::agreement_provenance,
            generated::agreements,
            generated::consumers,
            generated::transfers};
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--self-test") {
            run_self_tests();
            return 0;
        }
        if (argc != 2) {
            std::cerr << "usage: relocatable_world_matrix_check CLOSURE_JSON\n"
                         "       relocatable_world_matrix_check --self-test\n";
            return 2;
        }

        atomic_output destination(argv[1]);
        std::ofstream output(destination.temporary(),
                             std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("cannot create closure output");
        }
        const auto input = generated_view();
        write_closure(output, input, generated::agreements_sha256);
        output.close();
        if (!output) {
            throw std::runtime_error("cannot finish closure output");
        }
        destination.commit();
        const auto result = matrix::close_matrix(input);
        if (result.status == matrix::closure_status::pass) {
            std::cout << "WORKFLOW PASS: nodes=" << result.counts.nodes
                      << "; agreement_pairs=" << result.counts.pairs
                      << "; named_permits=" << result.counts.named_permits
                      << '/' << result.counts.named_decisions
                      << "; directed_loads=" << result.counts.passes
                      << '/' << result.counts.transfers << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "matrix error: " << error.what() << '\n';
        return 1;
    }
}
