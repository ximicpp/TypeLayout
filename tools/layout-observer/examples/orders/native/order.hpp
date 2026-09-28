#pragma once
#include <cstdint>

#ifndef ORDER_PACK
#define ORDER_PACK 4
#endif
#if ORDER_PACK == 1
#pragma pack(push, 1)
#elif ORDER_PACK == 4
#pragma pack(push, 4)
#else
#error Unsupported ORDER_PACK
#endif
namespace orders {
struct Order {
    std::uint8_t kind;
    std::int32_t id;
    std::int16_t units;
};
}
#pragma pack(pop)
