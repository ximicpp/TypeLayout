// consumer.cpp -- Native directed checkpoint consumer for matrix evidence.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "checkpoint.hpp"
#include "evidence_json.hpp"
#include "matrix_model.hpp"
#include "relocatable_world_consumer_input.hpp"
#include "world.hpp"
#include "world_runtime.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
namespace generated = relocatable_world_demo::generated::consumer_input;
namespace matrix = relocatable_world_demo::matrix;

using relocatable_world_demo::evidence_json::write_key;
using relocatable_world_demo::evidence_json::write_string;

template <typename T>
inline constexpr auto current_signature_storage =
    boost::typelayout::get_layout_signature<T>();

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

struct current_contract {
    std::array<bool, matrix::key_count> admission{};
    std::array<std::string_view, matrix::key_count> signatures{};
};

current_contract current_contract_facts() {
    current_contract result{};
    std::size_t index = 0;
    relocatable_world_demo::for_each_contract_type(
        [&]<typename T>(std::string_view) {
            const auto& signature = current_signature_storage<T>;
            result.admission[index] = boost::typelayout::is_admitted_v<
                T, relocatable_world_demo::whole_region_profile>;
            result.signatures[index] = {
                signature.value, signature.length()};
            ++index;
        });
    return result;
}

struct transfer_result {
    matrix::node_id producer{};
    matrix::transfer_status status{matrix::transfer_status::incomplete};
    std::string_view reason;
    bool provenance_digest_present{};
    std::string_view provenance_sha256;
    bool region_digest_present{};
    std::string_view region_sha256;
};

std::vector<std::byte> read_region(const fs::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("region file is unavailable");
    }
    const auto end = input.tellg();
    if (end < 0) {
        throw std::runtime_error("region file size is unavailable");
    }
    const auto size = static_cast<std::size_t>(end);
    std::vector<std::byte> bytes(size);
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!input) {
        throw std::runtime_error("region file cannot be read completely");
    }
    return bytes;
}

matrix::transfer_status map_rejection(
    relocatable_world_demo::rejection_layer layer) {
    switch (layer) {
    case relocatable_world_demo::rejection_layer::envelope:
        return matrix::transfer_status::reject_envelope;
    case relocatable_world_demo::rejection_layer::region:
        return matrix::transfer_status::reject_region;
    case relocatable_world_demo::rejection_layer::graph:
        return matrix::transfer_status::reject_graph;
    }
    return matrix::transfer_status::incomplete;
}

std::string_view rejection_reason(matrix::transfer_status status) {
    switch (status) {
    case matrix::transfer_status::reject_envelope:
        return "checkpoint envelope rejected";
    case matrix::transfer_status::reject_region:
        return "checkpoint region rejected";
    case matrix::transfer_status::reject_graph:
        return "checkpoint graph rejected";
    default:
        return "consumer evaluation incomplete";
    }
}

transfer_result evaluate_transfer(
    const matrix::producer_record& producer,
    const current_contract& current,
    const fs::path& evidence_root) {
    transfer_result result{};
    result.producer = producer.node;
    if (!producer.present) {
        result.reason = producer.error.empty()
            ? std::string_view{"producer evidence incomplete"}
            : producer.error;
        return result;
    }

    result.provenance_digest_present = true;
    result.provenance_sha256 = producer.provenance_sha256;
    result.status = matrix::load_after_typelayout_gate(
        current.admission, current.signatures, producer, [&] {
            if (!producer.region_present) {
                result.reason = "permitted producer has no region artifact";
                return matrix::transfer_status::incomplete;
            }
            result.region_digest_present = true;
            result.region_sha256 = producer.region_sha256;

            try {
                const fs::path filename(producer.region_filename);
                if (filename.empty() || filename.is_absolute() ||
                    filename.filename() != filename) {
                    throw std::runtime_error("region filename is not canonical");
                }
                const auto region_path = fs::canonical(evidence_root / filename);
                if (region_path.parent_path() != evidence_root) {
                    throw std::runtime_error("region file escapes evidence root");
                }
                const auto bytes = read_region(region_path);
                const auto loaded = relocatable_world_demo::load_checkpoint(bytes);
                if (!relocatable_world_demo::canonical_graph_matches(loaded) ||
                    relocatable_world_demo::world_root(loaded).tick != 42 ||
                    relocatable_world_demo::party_total_hp(loaded) != 420) {
                    result.reason = "canonical graph or business state differs";
                    return matrix::transfer_status::reject_graph;
                }
                result.reason =
                    "checkpoint loaded and canonical world validated";
                return matrix::transfer_status::pass;
            } catch (const relocatable_world_demo::checkpoint_error& error) {
                const auto status = map_rejection(error.layer());
                result.reason = rejection_reason(status);
                return status;
            } catch (const std::exception&) {
                result.reason = "region artifact unavailable or invalid";
                return matrix::transfer_status::incomplete;
            }
        });
    if (result.status == matrix::transfer_status::skipped_typelayout_reject) {
        result.reason = "TypeLayout Admission or Agreement rejected";
        if (producer.region_present) {
            result.region_digest_present = true;
            result.region_sha256 = producer.region_sha256;
        }
    }
    return result;
}

void write_nullable_string(std::ostream& output,
                           bool present,
                           std::string_view value) {
    if (present) {
        write_string(output, value);
    } else {
        output << "null";
    }
}

void write_build(std::ostream& output,
                 const matrix::consumer_build_record& build) {
    output << "{\n    ";
    write_key(output, "source_sha");
    write_string(output, build.run.source_sha);
    output << ",\n    ";
    write_key(output, "workflow_run");
    write_string(output, build.run.workflow_run);
    output << ",\n    ";
    write_key(output, "sources_sha256");
    write_string(output, build.run.sources_sha256);
    output << ",\n    ";
    write_key(output, "outputs_sha256");
    write_string(output, build.run.outputs_sha256);
    output << ",\n    ";
    write_key(output, "execution");
    write_string(output, build.execution);
    output << ",\n    ";
    write_key(output, "runner");
    write_string(output, build.runner);
    output << ",\n    ";
    write_key(output, "runner_image");
    write_string(output, build.runner_image);
    output << ",\n    ";
    write_key(output, "toolchain_artifact_sha256");
    write_string(output, build.toolchain_artifact_sha256);
    output << ",\n    ";
    write_key(output, "compiler_family");
    write_string(output, build.compiler_family);
    output << ",\n    ";
    write_key(output, "compiler_revision");
    write_string(output, build.compiler_revision);
    output << ",\n    ";
    write_key(output, "compiler_version");
    write_string(output, build.compiler_version);
    output << ",\n    ";
    write_key(output, "target");
    write_string(output, build.target);
    output << ",\n    ";
    write_key(output, "stdlib");
    write_string(output, build.stdlib);
    output << ",\n    ";
    write_key(output, "flags");
    write_string(output, build.flags);
    output << ",\n    ";
    write_key(output, "xcode_version");
    write_string(output, build.xcode_version);
    output << ",\n    ";
    write_key(output, "xcode_build");
    write_string(output, build.xcode_build);
    output << ",\n    ";
    write_key(output, "sdk_version");
    write_string(output, build.sdk_version);
    output << ",\n    ";
    write_key(output, "sdk_build");
    write_string(output, build.sdk_build);
    output << ",\n    ";
    write_key(output, "deployment_target");
    write_string(output, build.deployment_target);
    output << ",\n    ";
    write_key(output, "sdk_locked");
    output << (build.sdk_locked ? "true" : "false") << "\n  }";
}

void write_results(std::ostream& output,
                   std::span<const transfer_result> transfers) {
    output << "{\n  ";
    write_key(output, "schema");
    output << "1,\n  ";
    write_key(output, "profile");
    write_string(output, profile_name(generated::profile));
    output << ",\n  ";
    write_key(output, "consumer");
    write_string(output, matrix::name(generated::consumer));
    output << ",\n  ";
    write_key(output, "consumer_provenance_sha256");
    write_string(output, generated::consumer_provenance_sha256);
    output << ",\n  ";
    write_key(output, "build");
    write_build(output, generated::build);
    output << ",\n  ";
    write_key(output, "transfers");
    output << "[\n";
    for (std::size_t index = 0; index < transfers.size(); ++index) {
        const auto& transfer = transfers[index];
        output << "    {";
        write_key(output, "producer");
        write_string(output, matrix::name(transfer.producer));
        output << ", ";
        write_key(output, "status");
        write_string(output, matrix::name(transfer.status));
        output << ", ";
        write_key(output, "reason");
        write_string(output, transfer.reason);
        output << ", ";
        write_key(output, "producer_provenance_sha256");
        write_nullable_string(output, transfer.provenance_digest_present,
                              transfer.provenance_sha256);
        output << ", ";
        write_key(output, "region_sha256");
        write_nullable_string(output, transfer.region_digest_present,
                              transfer.region_sha256);
        output << "}" << (index + 1 == transfers.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: relocatable_world_consumer PROFILE NODE "
                     "EVIDENCE_DIR RESULTS_JSON\n";
        return 2;
    }
    try {
        if (std::string_view(argv[1]) != profile_name(generated::profile) ||
            std::string_view(argv[2]) != matrix::name(generated::consumer)) {
            throw std::runtime_error(
                "runtime profile/node differs from generated input");
        }
        const fs::path results_path(argv[4]);
        const auto expected_results_name =
            std::string(matrix::name(generated::consumer)) + ".results.json";
        if (results_path.filename() != expected_results_name) {
            throw std::runtime_error("results filename does not bind consumer");
        }
        if (!generated::consumer_provenance_present ||
            generated::consumer_provenance_sha256.empty()) {
            throw std::runtime_error(
                "consumer provenance unavailable; preserve fallback result");
        }
        const auto evidence_root = fs::canonical(argv[3]);
        if (evidence_root != fs::canonical(fs::path(generated::evidence_root))) {
            throw std::runtime_error(
                "runtime evidence directory differs from verified root");
        }
        const auto nodes = matrix::profile_nodes(generated::profile);
        if (generated::producers.size() != nodes.size()) {
            throw std::runtime_error("generated producer slot count differs");
        }
        const auto* own_provenance = matrix::unique_producer(
            generated::producers, generated::consumer);
        if (own_provenance == nullptr || !own_provenance->present ||
            own_provenance->provenance_sha256 !=
                generated::consumer_provenance_sha256) {
            throw std::runtime_error(
                "consumer provenance digest is not bound to its slot");
        }

        const auto current = current_contract_facts();
        std::array<transfer_result, 5> storage{};
        std::size_t transfer_count = 0;
        for (const auto node : nodes) {
            if (node == generated::consumer) {
                continue;
            }
            const auto* producer = matrix::unique_producer(
                generated::producers, node);
            if (producer == nullptr) {
                transfer_result missing{};
                missing.producer = node;
                missing.reason = "producer slot missing or duplicated";
                storage[transfer_count++] = missing;
                continue;
            }
            storage[transfer_count++] = evaluate_transfer(
                *producer, current, evidence_root);
        }
        const std::span<const transfer_result> transfers{
            storage.data(), transfer_count};

        atomic_output destination(results_path);
        std::ofstream output(destination.temporary(),
                             std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("cannot create consumer result");
        }
        write_results(output, transfers);
        output.close();
        if (!output) {
            throw std::runtime_error("cannot finish consumer result");
        }
        destination.commit();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "consumer error: " << error.what() << '\n';
        return 1;
    }
}
