#include "order.hpp"
#include <cstddef>
#include <iostream>

int main() {
    using orders::Order;
    std::cout << "{\"size\":" << sizeof(Order) << ",\"kind\":" << offsetof(Order, kind)
              << ",\"id\":" << offsetof(Order, id) << ",\"units\":" << offsetof(Order, units) << "}\n";
}
