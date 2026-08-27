// evidence_json.hpp -- Minimal deterministic JSON emission for demo evidence.
//
// Copyright (c) 2026 TypeLayout Development Team
// Distributed under the Boost Software License, Version 1.0.

#ifndef BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_EVIDENCE_JSON_HPP
#define BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_EVIDENCE_JSON_HPP

#include <ostream>
#include <string_view>

namespace relocatable_world_demo::evidence_json {

inline void write_string(std::ostream& output, std::string_view value) {
    constexpr char hexadecimal[] = "0123456789abcdef";
    output.put('"');
    for (const unsigned char character : value) {
        if (character == '"' || character == '\\') {
            output.put('\\');
            output.put(static_cast<char>(character));
        } else if (character < 0x20) {
            output << "\\u00"
                   << hexadecimal[(character >> 4) & 0x0f]
                   << hexadecimal[character & 0x0f];
        } else {
            output.put(static_cast<char>(character));
        }
    }
    output.put('"');
}

inline void write_key(std::ostream& output, std::string_view key) {
    write_string(output, key);
    output << ": ";
}

} // namespace relocatable_world_demo::evidence_json

#endif // BOOST_TYPELAYOUT_RELOCATABLE_WORLD_DEMO_EVIDENCE_JSON_HPP
