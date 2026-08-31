#include "unit.hpp"

#include <boost/typelayout/tools/sig_export.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: unit signature exporter OUTPUT_DIRECTORY\n";
        return 2;
    }

#if defined(TYPELAYOUT_RELOCATABLE_UNIT_PACKED_EFFECT)
    constexpr std::string_view platform_id = "unit_producer_packed";
#else
    constexpr std::string_view platform_id = "unit_producer_ok";
#endif

    boost::typelayout::SigExporter exporter{std::string(platform_id)};
    relocatable_unit_handoff_demo::for_each_unit_contract_type(
        [&]<typename T>(std::string_view key) {
            static_assert(boost::typelayout::is_admitted_v<
                T, boost::typelayout::TransferProfile::whole_region_relocation>);
            exporter.add_relocatable<T>(std::string(key));
        });

    const auto output = std::filesystem::path(argv[1]) /
        (std::string(platform_id) + ".sig.hpp");
    std::filesystem::create_directories(output.parent_path());
    return exporter.write(output.string());
}
