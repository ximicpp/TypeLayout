#include "world.hpp"

#include <cstdio>

int main() {
    static_assert(xoffset_world_demo::world_contract_admitted_v);

    xoffset_world_demo::relative_ptr<xoffset_world_demo::Entity> null_entity;
    const auto& const_null_entity = null_entity;
    if (null_entity.get() != nullptr || const_null_entity.get() != nullptr) {
        std::fprintf(stderr, "relative_ptr null resolution failed\n");
        return 1;
    }

    std::printf("Model: relative_ptr + four-type contract PASS\n");
    return 0;
}
