#pragma once
#include <cstdint>

namespace marshal_fixtures {
struct DefaultBoolChar { std::int32_t enabled; char16_t letter; std::int32_t count; };
struct ByteBoolChar { std::uint8_t enabled; char16_t letter; std::int32_t count; };
struct InlineInts { std::int32_t values[3]; std::int16_t code; };
}

#if defined(_WIN32)
#if defined(TYPELAYOUT_MARSHAL_BUILD)
#define TYPELAYOUT_MARSHAL_EXPORT extern "C" __declspec(dllexport)
#else
#define TYPELAYOUT_MARSHAL_EXPORT extern "C" __declspec(dllimport)
#endif
#define TYPELAYOUT_MARSHAL_CALL __cdecl
#else
#define TYPELAYOUT_MARSHAL_EXPORT extern "C" __attribute__((visibility("default")))
#define TYPELAYOUT_MARSHAL_CALL
#endif

TYPELAYOUT_MARSHAL_EXPORT std::uint64_t TYPELAYOUT_MARSHAL_CALL typelayout_marshal_size(int case_id);
TYPELAYOUT_MARSHAL_EXPORT std::uint64_t TYPELAYOUT_MARSHAL_CALL typelayout_marshal_offset(int case_id, int field_id);
TYPELAYOUT_MARSHAL_EXPORT int TYPELAYOUT_MARSHAL_CALL typelayout_marshal_validate_default(const marshal_fixtures::DefaultBoolChar* value);
TYPELAYOUT_MARSHAL_EXPORT int TYPELAYOUT_MARSHAL_CALL typelayout_marshal_validate_byte(const marshal_fixtures::ByteBoolChar* value);
TYPELAYOUT_MARSHAL_EXPORT int TYPELAYOUT_MARSHAL_CALL typelayout_marshal_validate_array(const marshal_fixtures::InlineInts* value);
