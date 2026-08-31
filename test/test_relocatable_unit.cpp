#include "unit.hpp"

#include <boost/typelayout.hpp>

#include <cstdint>
#include <string_view>
#include <type_traits>

using namespace boost::typelayout;
using namespace relocatable_unit_handoff_demo;

struct NativePointerUnit {
    Effect* effect;
};

static_assert(std::is_standard_layout_v<UnitPosition>);
static_assert(std::is_trivially_copyable_v<UnitPosition>);
static_assert(std::is_implicit_lifetime_v<UnitPosition>);
static_assert(std::is_standard_layout_v<Effect>);
static_assert(std::is_trivially_copyable_v<Effect>);
static_assert(std::is_implicit_lifetime_v<Effect>);
static_assert(std::is_standard_layout_v<UnitSnapshot>);
static_assert(std::is_trivially_copyable_v<UnitSnapshot>);
static_assert(std::is_implicit_lifetime_v<UnitSnapshot>);
static_assert(is_admitted_v<UnitSnapshot, whole_region_profile>);
static_assert(is_admitted_v<Effect, whole_region_profile>);
static_assert(is_admitted_v<EffectRelativePtr, whole_region_profile>);
static_assert(is_admitted_v<AttributeEntry, whole_region_profile>);
static_assert(unit_contract_admitted_v);
static_assert(!is_admitted_v<NativePointerUnit, whole_region_profile>);

inline constexpr auto effect_pointer_signature =
    get_layout_signature<EffectRelativePtr>();
inline constexpr auto effect_signature = get_layout_signature<Effect>();
static_assert(effect_pointer_signature.length() != 0);
static_assert(effect_signature.length() != 0);

int main() {
    const std::string_view pointer_text{
        effect_pointer_signature.value,
        effect_pointer_signature.length(),
    };
    const std::string_view effect_text{
        effect_signature.value,
        effect_signature.length(),
    };
    return pointer_text.find("opaque") == std::string_view::npos &&
                   effect_text.find("opaque") == std::string_view::npos
        ? 0
        : 1;
}
