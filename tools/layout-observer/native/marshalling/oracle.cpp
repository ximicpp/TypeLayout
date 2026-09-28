#include "fixtures.hpp"
#include <iostream>

int main() {
    using namespace marshal_fixtures;
    DefaultBoolChar default_value{1, u'\u4e2d', 0x12345678};
    ByteBoolChar byte_value{1, u'\u4e2d', 0x12345678};
    InlineInts array_value{{0x11223344, -7, 42}, -1234};
    if (typelayout_marshal_size(0) != 12 || typelayout_marshal_size(1) != 8 || typelayout_marshal_size(2) != 16) return 1;
    if (typelayout_marshal_offset(0, 1) != 4 || typelayout_marshal_offset(0, 2) != 8 ||
        typelayout_marshal_offset(1, 1) != 2 || typelayout_marshal_offset(1, 2) != 4 || typelayout_marshal_offset(2, 1) != 12) return 1;
    if (!typelayout_marshal_validate_default(&default_value) || !typelayout_marshal_validate_byte(&byte_value) || !typelayout_marshal_validate_array(&array_value)) return 1;
    default_value.enabled = 0; byte_value.letter = 0; array_value.values[1] = 7;
    if (typelayout_marshal_validate_default(&default_value) || typelayout_marshal_validate_byte(&byte_value) || typelayout_marshal_validate_array(&array_value)) return 1;
    if (typelayout_marshal_validate_default(nullptr) || typelayout_marshal_validate_byte(nullptr) || typelayout_marshal_validate_array(nullptr)) return 1;
    if (typelayout_marshal_size(-1) != UINT64_MAX || typelayout_marshal_offset(0, 3) != UINT64_MAX || typelayout_marshal_offset(8, 0) != UINT64_MAX) return 1;
    std::cout << "PASS native marshalling: sizes 12/8/16, offsets, exact sentinels, nulls and rejected mismatches\n";
}
