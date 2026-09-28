#include "fixtures.hpp"
#include <cstddef>
#include <limits>
#include <type_traits>

using namespace marshal_fixtures;
static_assert(std::is_standard_layout_v<DefaultBoolChar> && std::is_standard_layout_v<ByteBoolChar> && std::is_standard_layout_v<InlineInts>);
static_assert(sizeof(char16_t) == 2);

std::uint64_t TYPELAYOUT_MARSHAL_CALL typelayout_marshal_size(int case_id) {
    switch (case_id) {
    case 0: return sizeof(DefaultBoolChar);
    case 1: return sizeof(ByteBoolChar);
    case 2: return sizeof(InlineInts);
    default: return UINT64_MAX;
    }
}
std::uint64_t TYPELAYOUT_MARSHAL_CALL typelayout_marshal_offset(int case_id, int field_id) {
    switch (case_id) {
    case 0:
        switch (field_id) { case 0: return offsetof(DefaultBoolChar, enabled); case 1: return offsetof(DefaultBoolChar, letter); case 2: return offsetof(DefaultBoolChar, count); }
        break;
    case 1:
        switch (field_id) { case 0: return offsetof(ByteBoolChar, enabled); case 1: return offsetof(ByteBoolChar, letter); case 2: return offsetof(ByteBoolChar, count); }
        break;
    case 2:
        switch (field_id) { case 0: return offsetof(InlineInts, values); case 1: return offsetof(InlineInts, code); }
        break;
    }
    return UINT64_MAX;
}
int TYPELAYOUT_MARSHAL_CALL typelayout_marshal_validate_default(const DefaultBoolChar* value) {
    return value && value->enabled == 1 && value->letter == u'\u4e2d' && value->count == 0x12345678;
}
int TYPELAYOUT_MARSHAL_CALL typelayout_marshal_validate_byte(const ByteBoolChar* value) {
    return value && value->enabled == 1 && value->letter == u'\u4e2d' && value->count == 0x12345678;
}
int TYPELAYOUT_MARSHAL_CALL typelayout_marshal_validate_array(const InlineInts* value) {
    return value && value->values[0] == 0x11223344 && value->values[1] == -7 && value->values[2] == 42 && value->code == -1234;
}
