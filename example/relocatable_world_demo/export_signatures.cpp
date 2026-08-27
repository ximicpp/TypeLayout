#include "world.hpp"

#include <boost/typelayout/tools/sig_export.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

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

    boost::typelayout::SigExporter exporter{platform_id};
    relocatable_world_demo::for_each_contract_type(
        [&]<typename T>(std::string_view key) {
            static_assert(boost::typelayout::is_admitted_v<
                T, boost::typelayout::TransferProfile::whole_region_relocation>);
            exporter.add_relocatable<T>(std::string(key));
        });

    const auto output = std::filesystem::path(argv[1]) /
        (platform_id + ".sig.hpp");
    std::filesystem::create_directories(output.parent_path());
    return exporter.write(output.string());
}
