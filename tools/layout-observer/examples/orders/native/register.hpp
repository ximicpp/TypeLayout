#pragma once
#include "order.hpp"

inline void register_layouts(observer::Collector& collector) {
    collector.collect<orders::Order>("orders.native");
}
