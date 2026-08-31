#include "world.hpp"
#include "../relocatable_unit_handoff_demo/unit.hpp"

#include <boost/typelayout/tools/sig_export.hpp>

#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

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

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: signature exporter OUTPUT_DIRECTORY "
                     "[PLATFORM_ID]\n";
        return 2;
    }

#if defined(TYPELAYOUT_RELOCATABLE_WORLD_PACKED_ENTITY)
    constexpr std::string_view default_platform_id = "producer_packed";
#else
    constexpr std::string_view default_platform_id = "producer_ok";
#endif
    const std::string platform_id = argc == 3
        ? std::string(argv[2])
        : std::string(default_platform_id);
    if (argc == 3 && !is_matrix_node(platform_id)) {
        std::cerr << "matrix PLATFORM_ID must be one of the fixed six nodes\n";
        return 2;
    }

    boost::typelayout::SigExporter exporter{platform_id};
    relocatable_world_demo::for_each_contract_type(
        [&]<typename T>(std::string_view key) {
            static_assert(boost::typelayout::is_admitted_v<
                T, boost::typelayout::TransferProfile::whole_region_relocation>);
            exporter.add_relocatable<T>(std::string(key));
        });
    if (argc == 3) {
        relocatable_unit_handoff_demo::for_each_unit_contract_type(
            [&]<typename T>(std::string_view key) {
                static_assert(boost::typelayout::is_admitted_v<
                    T,
                    boost::typelayout::TransferProfile::whole_region_relocation>);
                exporter.add_relocatable<T>(std::string(key));
            });
    }

    const auto output = std::filesystem::path(argv[1]) /
        (platform_id + ".sig.hpp");
    std::filesystem::create_directories(output.parent_path());
    return exporter.write(output.string());
}
