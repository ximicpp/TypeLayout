// agreement_check.cpp -- C++20-only relocatable-world Agreement writer.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "evidence_json.hpp"
#include "matrix_model.hpp"
#include "relocatable_world_agreement_input.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

namespace fs = std::filesystem;
namespace generated = relocatable_world_demo::generated::agreement_input;
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

std::string_view agreement_reason(
    std::span<const matrix::producer_record> producers,
    matrix::node_id left,
    matrix::node_id right,
    matrix::scenario_id scenario,
    matrix::key_id key) {
    const auto* left_record = matrix::unique_producer(producers, left);
    const auto* right_record = matrix::unique_producer(producers, right);
    const auto* left_contract = left_record == nullptr
        ? nullptr : matrix::contract_for(*left_record, scenario);
    const auto* right_contract = right_record == nullptr
        ? nullptr : matrix::contract_for(*right_record, scenario);
    std::size_t key_index = matrix::key_count;
    const auto scenario_index = static_cast<std::size_t>(scenario);
    if (scenario_index < matrix::scenario_count) {
        for (std::size_t index = 0; index < matrix::key_count; ++index) {
            if (matrix::scenario_key_ids[scenario_index][index] == key) {
                key_index = index;
            }
        }
    }
    if (left_record == nullptr || right_record == nullptr ||
        !left_record->present || !right_record->present ||
        left_contract == nullptr || right_contract == nullptr ||
        key_index == matrix::key_count ||
        left_contract->signatures[key_index].empty() ||
        right_contract->signatures[key_index].empty()) {
        return "producer evidence incomplete";
    }
    if (!left_contract->admission[key_index] ||
        !right_contract->admission[key_index]) {
        return "Admission rejected";
    }
    if (left_contract->signatures[key_index] !=
        right_contract->signatures[key_index]) {
        return "layout signature differs";
    }
    return "Admission and signature agree";
}

void write_agreements(std::ostream& output) {
    const auto nodes = matrix::profile_nodes(generated::profile);
    const std::span<const matrix::producer_record> producers =
        generated::producers;

    output << "{\n  ";
    write_key(output, "schema");
    output << "2,\n  ";
    write_key(output, "profile");
    write_string(output, profile_name(generated::profile));
    output << ",\n  ";
    write_key(output, "producer_provenance_sha256");
    output << "{\n";
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const auto node = nodes[index];
        const auto* producer = matrix::unique_producer(producers, node);
        output << "    ";
        write_key(output, matrix::name(node));
        if (producer != nullptr && producer->present) {
            write_string(output, producer->provenance_sha256);
        } else {
            output << "null";
        }
        output << (index + 1 == nodes.size() ? "\n" : ",\n");
    }
    output << "  },\n  ";
    write_key(output, "pairs");
    output << "[\n";

    std::size_t pair_index = 0;
    const auto pair_count = matrix::expected_pair_count(generated::profile);
    for (std::size_t left_index = 0; left_index < nodes.size(); ++left_index) {
        for (std::size_t right_index = left_index + 1;
             right_index < nodes.size(); ++right_index) {
            const auto left = nodes[left_index];
            const auto right = nodes[right_index];
            output << "    {\n      ";
            write_key(output, "left");
            write_string(output, matrix::name(left));
            output << ",\n      ";
            write_key(output, "right");
            write_string(output, matrix::name(right));
            output << ",\n      ";
            write_key(output, "scenarios");
            output << "{\n";
            for (std::size_t scenario_index = 0;
                 scenario_index < matrix::scenario_count; ++scenario_index) {
                const auto scenario = matrix::scenario_ids[scenario_index];
                output << "        ";
                write_key(output, matrix::name(scenario));
                output << "[\n";
                for (std::size_t key_index = 0;
                     key_index < matrix::key_count; ++key_index) {
                    const auto key =
                        matrix::scenario_key_ids[scenario_index][key_index];
                    const auto status = matrix::compute_agreement(
                        producers, left, right, scenario, key);
                    output << "          {";
                    write_key(output, "key");
                    write_string(output, matrix::name(key));
                    output << ", ";
                    write_key(output, "status");
                    write_string(output, matrix::name(status));
                    output << ", ";
                    write_key(output, "reason");
                    write_string(output, agreement_reason(
                        producers, left, right, scenario, key));
                    output << "}"
                           << (key_index + 1 == matrix::key_count
                                   ? "\n" : ",\n");
                }
                output << "        ]"
                       << (scenario_index + 1 == matrix::scenario_count
                               ? "\n" : ",\n");
            }
            output << "      }\n    }";
            ++pair_index;
            output << (pair_index == pair_count ? "\n" : ",\n");
        }
    }
    output << "  ]\n}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: relocatable_world_agreement_check AGREEMENTS_JSON\n";
        return 2;
    }
    try {
        atomic_output destination(argv[1]);
        std::ofstream output(destination.temporary(),
                             std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("cannot create Agreement output");
        }
        write_agreements(output);
        output.close();
        if (!output) {
            throw std::runtime_error("cannot finish Agreement output");
        }
        destination.commit();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Agreement error: " << error.what() << '\n';
        return 1;
    }
}
