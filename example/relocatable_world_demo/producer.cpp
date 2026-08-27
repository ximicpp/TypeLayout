// producer.cpp -- Native producer role for relocatable-world evidence.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#include "checkpoint.hpp"
#include "evidence_json.hpp"
#include "world.hpp"
#include "world_runtime.hpp"

#include <boost/typelayout.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

namespace fs = std::filesystem;

constexpr std::array<std::string_view, 6> matrix_nodes = {
    "x86_64_linux_gcc",
    "x86_64_linux_clang",
    "arm64_linux_gcc",
    "arm64_linux_clang",
    "arm64_macos_clang",
    "x86_64_macos_clang",
};

constexpr bool is_matrix_node(std::string_view value) {
    for (const auto node : matrix_nodes) {
        if (node == value) {
            return true;
        }
    }
    return false;
}

class temporary_output {
public:
    explicit temporary_output(fs::path destination)
        : destination_(std::move(destination)),
          temporary_(destination_.string() + ".tmp") {
        std::error_code ignored;
        fs::remove(temporary_, ignored);
    }

    temporary_output(const temporary_output&) = delete;
    temporary_output& operator=(const temporary_output&) = delete;

    ~temporary_output() {
        if (!committed_) {
            std::error_code ignored;
            fs::remove(temporary_, ignored);
        }
    }

    const fs::path& path() const noexcept { return temporary_; }

    void commit() {
        // Every supported matrix node is POSIX; same-directory rename atomically
        // replaces an earlier regular-file result on those platforms.
        fs::rename(temporary_, destination_);
        committed_ = true;
    }

private:
    fs::path destination_;
    fs::path temporary_;
    bool committed_ = false;
};

void write_facts(const fs::path& destination, std::string_view node) {
    temporary_output output_file(destination);
    std::ofstream output(output_file.path(),
                         std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot create producer facts temporary file");
    }

    using relocatable_world_demo::evidence_json::write_key;
    using relocatable_world_demo::evidence_json::write_string;

    output << "{\n";
    output << "  ";
    write_key(output, "schema");
    output << "1,\n";
    output << "  ";
    write_key(output, "node");
    write_string(output, node);
    output << ",\n";
    output << "  ";
    write_key(output, "admission");
    output << "{\n";
    bool first = true;
    relocatable_world_demo::for_each_contract_type(
        [&]<typename T>(std::string_view key) {
            if (!first) {
                output << ",\n";
            }
            output << "    ";
            write_key(output, key);
            output << (boost::typelayout::is_admitted_v<
                           T, relocatable_world_demo::whole_region_profile>
                           ? "true"
                           : "false");
            first = false;
        });
    output << "\n  },\n";
    output << "  ";
    write_key(output, "signatures");
    output << "{\n";
    first = true;
    relocatable_world_demo::for_each_contract_type(
        [&]<typename T>(std::string_view key) {
            constexpr auto signature =
                boost::typelayout::get_layout_signature<T>();
            if (!first) {
                output << ",\n";
            }
            output << "    ";
            write_key(output, key);
            write_string(output,
                         std::string_view(signature.value, signature.size));
            first = false;
        });
    output << "\n  }\n";
    output << "}\n";
    output.close();
    if (!output) {
        throw std::runtime_error("cannot finish producer facts temporary file");
    }
    output_file.commit();
}

void write_region(const fs::path& destination,
                  std::span<const std::byte> artifact) {
    temporary_output output_file(destination);
    std::ofstream output(output_file.path(),
                         std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot create region temporary file");
    }
    output.write(reinterpret_cast<const char*>(artifact.data()),
                 static_cast<std::streamsize>(artifact.size()));
    output.close();
    if (!output) {
        throw std::runtime_error("cannot finish region temporary file");
    }
    output_file.commit();
}

void remove_stale_payloads(const fs::path& directory,
                           std::string_view node) {
    for (const auto suffix : {std::string_view{".sig.hpp"},
                              std::string_view{".region"}}) {
        std::error_code error;
        fs::remove(directory /
                       (std::string(node) + std::string(suffix)),
                   error);
        if (error) {
            throw fs::filesystem_error(
                "cannot remove stale rejected producer payload", error);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: relocatable_world_producer NODE OUTPUT_DIRECTORY\n";
        return 2;
    }

    const std::string_view node = argv[1];
    if (!is_matrix_node(node)) {
        std::cerr << "NODE must be one of the fixed six matrix nodes\n";
        return 2;
    }

    try {
        const fs::path output_directory = argv[2];
        fs::create_directories(output_directory);
        write_facts(
            output_directory /
                (std::string(node) + ".producer-facts.json"),
            node);

        if constexpr (!relocatable_world_demo::world_contract_admitted_v) {
            remove_stale_payloads(output_directory, node);
            std::cout << "PRODUCER REJECT node=" << node
                      << " payload omitted\n";
            return 0;
        } else {
            const auto checkpoint = relocatable_world_demo::save_checkpoint(
                relocatable_world_demo::build_canonical_world());
            write_region(
                output_directory / (std::string(node) + ".region"),
                checkpoint);
            std::cout << "PRODUCER READY node=" << node
                      << " admission=4/4 region=" << node << ".region\n";
            return 0;
        }
    } catch (const std::exception& error) {
        std::cerr << "producer error: " << error.what() << '\n';
        return 1;
    }
}
