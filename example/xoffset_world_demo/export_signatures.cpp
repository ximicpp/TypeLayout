#include "world.hpp"

#include <boost/typelayout/tools/sig_export.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: signature exporter OUTPUT_DIRECTORY\n";
        return 2;
    }

    static_assert(xoffset_world_demo::world_contract_admitted_v);
#if defined(TYPELAYOUT_XOFFSET_PACKED_ENTITY)
    constexpr std::string_view producer_name = "producer_packed";
#else
    constexpr std::string_view producer_name = "producer_ok";
#endif

    boost::typelayout::SigExporter exporter{std::string(producer_name)};
    xoffset_world_demo::for_each_contract_type(
        [&]<typename T>(std::string_view key) {
            static_assert(boost::typelayout::is_admitted_v<
                T,
                boost::typelayout::TransferProfile::whole_region_relocation>);
            exporter.add_relocatable<T>(std::string(key));
        });

    const auto output = std::filesystem::path(argv[1]) /
        (std::string(producer_name) + ".sig.hpp");
    std::filesystem::create_directories(output.parent_path());
    return exporter.write(output.string());
}
