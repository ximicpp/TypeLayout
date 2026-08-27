# XOffset World Checkpoint Demo Implementation Plan

> **SUPERSEDED — DO NOT EXECUTE**
>
> This file is retained only as historical design context. Do not execute any task in this plan and do not restore an XOffset build or runtime dependency. The approved standalone design is `docs/superpowers/specs/2026-08-27-relocatable-world-demo-design.md`; its current implementation plans are `docs/superpowers/plans/2026-08-27-relocatable-world-demo-implementation.md` and `docs/superpowers/plans/2026-08-27-relocatable-world-matrix-implementation.md`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the smallest practical XOffset-backed TypeLayout demo that proves whole-region Admission, four-representation Agreement, no-fixup relative-pointer relocation, useful read/write behavior, and three layer-specific rejections.

**Architecture:** Extend only TypeLayout's Admission layer with address-context and transfer-profile traits. Keep the world model, exact XOffset adapter, finite contract list, runtime graph validation, and Agreement helper local to the demo; use existing `SigExporter` artifacts and XOffset's verified v1 loader rather than changing either subsystem.

**Tech Stack:** C++26 P2996 static reflection, Boost.TypeLayout headers, XOffsetDatastructure `next_cpp26`, CMake/CTest, Bloomberg Clang P2996 under WSL.

**Spec:** `docs/superpowers/specs/2026-08-27-xoffset-world-demo-design.md`

## Global Constraints

- Use `vendor/XOffsetDatastructure` from branch `next_cpp26` at `2233004983cd42664e3d6084ec09092b2968ad4e`; do not edit vendor source.
- `relative_ptr<T>` is one reflected `std::int32_t`, size 4, alignment 4, standard-layout, trivially copyable, and contains no opaque signature token.
- Do not change `TypeEntry`, `SigExporter`, generated fixture format, `CompatReporter`, or opaque registration.
- The only evidence keys are `WorldSnapshot`, `Entity`, `EntityRelativePtr`, and `EntityIndexEntry`.
- Admission for `Entity` and `WorldSnapshot` must be a conjunction over their exact member representations; no unconditional aggregate opt-in.
- Call `load_verified<WorldSnapshot>()` only after the positive Admission and Agreement gates pass.
- Validate every application relative delta as an integer before forming or dereferencing an `Entity*`.
- Finish all entity and party allocation before linking; do not grow/reorder those containers or invoke `XCompactor` after linking.
- Build the demo only with P2996 Clang and only when the checked-out vendor header exists; leave existing GCC and compatibility jobs unchanged.
- Do not edit the deck in this implementation phase.
- Preserve the user's existing modifications to the CppCon deck design and implementation-plan documents.

## File Map

- `.gitmodules` and `vendor/XOffsetDatastructure`: already-prepared dependency pin; commit without vendor edits.
- `include/boost/typelayout/admission.hpp`: public `SourceContext`, `TransferProfile`, customization traits, join operation, and `is_admitted_v`.
- `test/test_core.cpp`: compile-time coverage for context propagation and both transfer profiles.
- `example/xoffset_world_demo/world.hpp`: `relative_ptr`, world types, XOffset schema name, exact TypeLayout adapter, four-type contract iteration, and compile-time invariants.
- `example/xoffset_world_demo/export_signatures.cpp`: one exporter source compiled as normal and packed producers.
- `example/xoffset_world_demo/sigs/producer_ok.sig.hpp`: checked-in normal producer evidence.
- `example/xoffset_world_demo/sigs/producer_packed.sig.hpp`: checked-in `#pragma pack(1)` producer evidence.
- `example/xoffset_world_demo/demo.cpp`: Agreement helper, construction, graph validation, relocation flow, business query/mutation, negative cases, and concise output.
- `CMakeLists.txt`: guarded demo, exporter, and CTest targets.

---

### Task 1: Commit the XOffset `next_cpp26` Dependency Baseline

**Files:**
- Commit: `.gitmodules`
- Commit: `vendor/XOffsetDatastructure` gitlink

**Interfaces:**
- Consumes: the already-populated vendor checkout.
- Produces: a repository-visible dependency at the exact revision used by every later task.

- [ ] **Step 1: Verify the prepared dependency without modifying it**

Run from the worktree root:

```powershell
Get-Content -Raw .gitmodules
git -C vendor/XOffsetDatastructure branch --show-current
git -C vendor/XOffsetDatastructure rev-parse HEAD
git -C vendor/XOffsetDatastructure status --short
git ls-files --stage -- .gitmodules vendor/XOffsetDatastructure
```

Expected:

```text
branch = next_cpp26
next_cpp26
2233004983cd42664e3d6084ec09092b2968ad4e
```

The vendor status output must be empty and the gitlink mode must be `160000`.

- [ ] **Step 2: Commit only the dependency entries**

```powershell
git commit --only -m "chore: vendor XOffset next_cpp26" -- .gitmodules vendor/XOffsetDatastructure
```

Verify that the commit contains exactly those two paths:

```powershell
git show --stat --oneline --summary HEAD
```

---

### Task 2: Add Profile-Aware TypeLayout Admission

**Files:**
- Modify: `test/test_core.cpp` in the test-type declarations and Admission assertion sections
- Modify: `include/boost/typelayout/admission.hpp` after the existing `is_byte_copy_safe_v` API

**Interfaces:**
- Consumes: existing `is_byte_copy_safe_v<T>` and P2996 member/base reflection helpers.
- Produces:
  - `enum class SourceContext { independent, same_region, address_space_dependent }`
  - `enum class TransferProfile { ordinary_copy, whole_region_relocation }`
  - `constexpr SourceContext join_source_context(SourceContext, SourceContext) noexcept`
  - `template<class T> struct source_context_traits`
  - `template<class T> inline constexpr SourceContext source_context_v`
  - `template<class T> struct region_relocation_traits`
  - `template<class T, TransferProfile P> inline constexpr bool is_admitted_v`

- [ ] **Step 1: Add compile-time tests before the new API exists**

Add these test types next to the existing inline test types in `test/test_core.cpp`:

```cpp
struct RegionOffset { std::int32_t delta; };
struct SameRegionEnvelope { std::uint32_t id; RegionOffset link; };
struct NonTrivialNoOptIn {
    std::int32_t value;
    ~NonTrivialNoOptIn() {}
};
struct NonTrivialRegion {
    std::int32_t value;
    ~NonTrivialRegion() {}
};

namespace boost { namespace typelayout { inline namespace v1 {
template <>
struct source_context_traits<::RegionOffset>
    : std::integral_constant<SourceContext, SourceContext::same_region> {};

template <>
struct region_relocation_traits<::NonTrivialRegion> {
    static constexpr bool enabled = true;
};
}}}
```

Add this assertion block after the existing Admission assertions:

```cpp
static_assert(join_source_context(SourceContext::independent,
                                  SourceContext::same_region) ==
              SourceContext::same_region);
static_assert(join_source_context(SourceContext::same_region,
                                  SourceContext::address_space_dependent) ==
              SourceContext::address_space_dependent);
static_assert(source_context_v<std::uint32_t> == SourceContext::independent);
static_assert(source_context_v<int*> == SourceContext::address_space_dependent);
static_assert(source_context_v<RegionOffset> == SourceContext::same_region);
static_assert(source_context_v<SameRegionEnvelope> == SourceContext::same_region);
static_assert(source_context_v<WithPtr> == SourceContext::address_space_dependent);

static_assert(is_admitted_v<Flat, TransferProfile::ordinary_copy>);
static_assert(!is_admitted_v<RegionOffset, TransferProfile::ordinary_copy>);
static_assert(is_admitted_v<RegionOffset,
                            TransferProfile::whole_region_relocation>);
static_assert(!is_admitted_v<WithPtr,
                             TransferProfile::whole_region_relocation>);
static_assert(is_byte_copy_safe_v<NonTrivialNoOptIn>);
static_assert(!is_admitted_v<NonTrivialNoOptIn,
                             TransferProfile::whole_region_relocation>);
static_assert(is_admitted_v<NonTrivialRegion,
                            TransferProfile::whole_region_relocation>);
```

- [ ] **Step 2: Build `test_core` and confirm the test fails for the missing API**

Configure once if `build-xoffset-demo` does not exist:

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && cmake -S . -B build-xoffset-demo -DCMAKE_CXX_COMPILER=/root/clang-p2996-install/bin/clang++ -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++" -DTYPELAYOUT_BUILD_COMPAT_CI=OFF'
```

Then run:

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-xoffset-demo --target test_core -j2'
```

Expected: compilation fails because `SourceContext`, `source_context_traits`, and `is_admitted_v` are not yet declared.

- [ ] **Step 3: Implement source-context calculation and profile predicates**

In `admission.hpp`, add the enums and this explicit join behavior:

```cpp
constexpr SourceContext join_source_context(
    SourceContext lhs, SourceContext rhs) noexcept {
    if (lhs == SourceContext::address_space_dependent ||
        rhs == SourceContext::address_space_dependent) {
        return SourceContext::address_space_dependent;
    }
    if (lhs == SourceContext::same_region ||
        rhs == SourceContext::same_region) {
        return SourceContext::same_region;
    }
    return SourceContext::independent;
}
```

Forward-declare `source_context_traits<T>`, then implement a `detail::source_context_impl<T>()` decision tree with these exact branches:

```text
pointer, reference, or member pointer -> address_space_dependent
polymorphic class -> address_space_dependent
array -> source_context_traits<element>::value
class or union -> join every reflected base and non-static data member
all other representation types -> independent
```

The base/member recursion must call `source_context_traits<std::remove_cv_t<FieldType>>::value`, so a user specialization such as `RegionOffset` is honored inside an aggregate. Use the existing unchecked P2996 access context and compile-time index recursion already used by `is_byte_copy_safe_impl`.

Define the primary traits and profile helper as:

```cpp
template <typename T>
struct source_context_traits
    : std::integral_constant<SourceContext,
          detail::source_context_impl<T>()> {};

template <typename T>
inline constexpr SourceContext source_context_v =
    source_context_traits<std::remove_cv_t<T>>::value;

template <typename T>
struct region_relocation_traits {
    static constexpr bool enabled =
        std::is_trivially_copyable_v<std::remove_cv_t<T>>;
};

template <typename T, TransferProfile Profile>
consteval bool is_admitted_impl() {
    using Bare = std::remove_cv_t<T>;
    if constexpr (Profile == TransferProfile::ordinary_copy) {
        return std::is_trivially_copyable_v<Bare> &&
               is_byte_copy_safe_v<Bare> &&
               source_context_v<Bare> == SourceContext::independent;
    } else {
        return is_byte_copy_safe_v<Bare> &&
               region_relocation_traits<Bare>::enabled &&
               source_context_v<Bare> !=
                   SourceContext::address_space_dependent;
    }
}

template <typename T, TransferProfile Profile>
inline constexpr bool is_admitted_v =
    detail::is_admitted_impl<T, Profile>();
```

Keep implementation helpers in `detail` and public names in `boost::typelayout::inline namespace v1`.

- [ ] **Step 4: Build and run the focused core test**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-xoffset-demo --target test_core -j2 && ctest --test-dir build-xoffset-demo -R "^test_core$" --output-on-failure'
```

Expected: build succeeds and `test_core` passes.

- [ ] **Step 5: Commit the Admission API and tests**

```powershell
git add -- include/boost/typelayout/admission.hpp test/test_core.cpp
git commit -m "feat: add profile-aware transfer admission"
```

---

### Task 3: Add the Minimal World Model and Relative Pointer

**Files:**
- Create: `example/xoffset_world_demo/world.hpp`
- Create: `example/xoffset_world_demo/demo.cpp`
- Modify: `CMakeLists.txt` after the core test targets

**Interfaces:**
- Consumes: Task 2's Admission API and XOffset's public `xoffsetdatastructure.hpp`.
- Produces:
  - `template<class T> class xoffset_world_demo::relative_ptr`
  - `Position`, `EntityKind`, `Entity`, `WorldSnapshot`, and `EntityIndexEntry`
  - `template<class F> constexpr void for_each_contract_type(F&&)`
  - `inline constexpr bool world_contract_admitted_v`

- [ ] **Step 1: Add a guarded compile target with a missing model include**

Create the initial `demo.cpp`:

```cpp
#include "world.hpp"

#include <cstdio>

int main() {
    static_assert(xoffset_world_demo::world_contract_admitted_v);
    std::printf("Model: relative_ptr + four-type contract PASS\n");
    return 0;
}
```

Add this guarded CMake block. A checkout without initialized submodules prints a status message instead of breaking unrelated Clang jobs:

```cmake
set(TYPELAYOUT_XOFFSET_HEADER
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/XOffsetDatastructure/xoffsetdatastructure.hpp")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(EXISTS "${TYPELAYOUT_XOFFSET_HEADER}")
        add_executable(xoffset_world_demo
            example/xoffset_world_demo/demo.cpp)
        target_link_libraries(xoffset_world_demo PRIVATE typelayout)
        target_include_directories(xoffset_world_demo PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/example/xoffset_world_demo
            ${CMAKE_CURRENT_SOURCE_DIR}/vendor/XOffsetDatastructure)
        add_test(NAME xoffset_world_demo COMMAND xoffset_world_demo)
        set_tests_properties(xoffset_world_demo PROPERTIES
            LABELS "typelayout;example;xoffset")
    else()
        message(STATUS
            "Skipping xoffset_world_demo: initialize vendor/XOffsetDatastructure")
    endif()
endif()
```

Reconfigure and build:

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && cmake -S . -B build-xoffset-demo -DCMAKE_CXX_COMPILER=/root/clang-p2996-install/bin/clang++ -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++" -DTYPELAYOUT_BUILD_COMPAT_CI=OFF && cmake --build build-xoffset-demo --target xoffset_world_demo -j2'
```

Expected: compilation fails because `world.hpp` does not exist.

- [ ] **Step 2: Implement `relative_ptr<T>` and the data model**

Create `world.hpp` with the project header first, then XOffset, then standard headers. Define `relative_ptr<T>` with exactly this public surface:

```cpp
template <typename T>
class relative_ptr {
public:
    constexpr relative_ptr() noexcept = default;
    constexpr std::int32_t raw_delta() const noexcept;
    constexpr explicit operator bool() const noexcept;
    T* get() noexcept;
    const T* get() const noexcept;
    void reset(T* target, std::span<const std::byte> region);

private:
    std::int32_t delta_ = 0;
};
```

Use the address of `delta_` as the anchor. `reset` must:

1. encode null as zero;
2. convert the region begin, anchor, and target to `std::uintptr_t`;
3. reject an overflowing region end;
4. require the full `delta_` and full `T` object ranges to be inside the region;
5. compute positive and negative magnitudes without unsigned wraparound;
6. reject magnitudes outside `std::int32_t`'s representable range;
7. assign the checked signed delta.

Define the approved model and `using EntityIndexEntry = XOffsetDatastructure::XKeyValue<std::uint64_t, std::uint32_t>`. Surround only `Entity` with `#pragma pack(push, 1)`/`#pragma pack(pop)` when `TYPELAYOUT_XOFFSET_PACKED_ENTITY` is defined. Register this stable wire name at global namespace scope:

```cpp
XOFFSET_REGISTER_SCHEMA_NAME(
    xoffset_world_demo::WorldSnapshot,
    "boost.typelayout.xoffset_world.v1")
```

- [ ] **Step 3: Add the exact demo-local TypeLayout adapter**

Specialize `source_context_traits` and `region_relocation_traits` only for:

```text
relative_ptr<T>
XString
XVector<Entity>
XVector<relative_ptr<Entity>>
XMap<uint64_t, uint32_t>
Entity
WorldSnapshot
```

Use these exact formulas with `P = TransferProfile::whole_region_relocation`:

```cpp
region_relocation_traits<XVector<Entity>>::enabled =
    is_admitted_v<Entity, P>;

region_relocation_traits<XVector<relative_ptr<Entity>>>::enabled =
    is_admitted_v<relative_ptr<Entity>, P>;

region_relocation_traits<XMap<std::uint64_t, std::uint32_t>>::enabled =
    is_admitted_v<EntityIndexEntry, P>;
```

`Entity` and `WorldSnapshot` use the full conjunctions from section 6.3 of the spec. The container source contexts join `same_region` with their element/key/value contexts. `relative_ptr<T>` returns `same_region` without inspecting `T`.

Add a local, explicit contract iterator:

```cpp
template <typename F>
constexpr void for_each_contract_type(F&& fn) {
    fn.template operator()<WorldSnapshot>("WorldSnapshot");
    fn.template operator()<Entity>("Entity");
    fn.template operator()<relative_ptr<Entity>>("EntityRelativePtr");
    fn.template operator()<EntityIndexEntry>("EntityIndexEntry");
}
```

Define `world_contract_admitted_v` as the conjunction of `is_admitted_v<T, whole_region_relocation>` for those same four types.

- [ ] **Step 4: Add compile-time representation and XOffset checks**

Place these checks after all specializations:

```cpp
using EntityRelativePtr = relative_ptr<Entity>;
static_assert(sizeof(EntityRelativePtr) == 4);
static_assert(alignof(EntityRelativePtr) == 4);
static_assert(std::is_standard_layout_v<EntityRelativePtr>);
static_assert(std::is_trivially_copyable_v<EntityRelativePtr>);
inline constexpr auto entity_relative_ptr_signature =
    boost::typelayout::get_layout_signature<EntityRelativePtr>();
static_assert(!entity_relative_ptr_signature.contains(
    boost::typelayout::FixedString{"O("}));
static_assert(!boost::typelayout::is_admitted_v<
    EntityRelativePtr,
    boost::typelayout::TransferProfile::ordinary_copy>);
static_assert(world_contract_admitted_v);
static_assert(XOffsetDatastructure::is_v1_wire_admitted_v<WorldSnapshot>);
```

- [ ] **Step 5: Build and run the model smoke test**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-xoffset-demo --target xoffset_world_demo -j2 && ctest --test-dir build-xoffset-demo -R "^xoffset_world_demo$" --output-on-failure'
```

Expected output includes:

```text
Model: relative_ptr + four-type contract PASS
```

- [ ] **Step 6: Commit the model and guarded target**

```powershell
git add -- CMakeLists.txt example/xoffset_world_demo/world.hpp example/xoffset_world_demo/demo.cpp
git commit -m "feat: add minimal XOffset world model"
```

---

### Task 4: Generate Producer Evidence and Gate on Agreement

**Files:**
- Create: `example/xoffset_world_demo/export_signatures.cpp`
- Create: `example/xoffset_world_demo/sigs/producer_ok.sig.hpp`
- Create: `example/xoffset_world_demo/sigs/producer_packed.sig.hpp`
- Modify: `example/xoffset_world_demo/demo.cpp`
- Modify: `CMakeLists.txt` inside the guarded XOffset block

**Interfaces:**
- Consumes: `for_each_contract_type`, `world_contract_admitted_v`, existing `SigExporter`, and `TypeEntry` fixtures.
- Produces:
  - `enum class AgreementResult { match, differ, incomplete }`
  - `AgreementResult check_agreement(PlatformInfo)`
  - normal and packed producer evidence with identical stable keys.

- [ ] **Step 1: Write the Agreement assertions before fixtures exist**

In `demo.cpp`, include both future fixture headers and add a local helper declaration:

```cpp
#include "sigs/producer_ok.sig.hpp"
#include "sigs/producer_packed.sig.hpp"

enum class AgreementResult { match, differ, incomplete };

AgreementResult check_agreement(
    boost::typelayout::PlatformInfo producer);
```

Update `main` to require:

```cpp
const auto ok = check_agreement(
    boost::typelayout::platform::producer_ok::get_platform_info());
const auto packed = check_agreement(
    boost::typelayout::platform::producer_packed::get_platform_info());
if (ok != AgreementResult::match ||
    packed != AgreementResult::differ) {
    return 1;
}
```

Build `xoffset_world_demo`. Expected: compilation fails because the two fixture headers do not exist.

- [ ] **Step 2: Implement the dual-mode exporter**

Create `export_signatures.cpp`. It must choose its producer name from the compile definition, assert Admission, register all four types with `add_relocatable<T>()`, and write one exact path:

```cpp
int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: signature exporter OUTPUT_DIRECTORY\n";
        return 2;
    }

    static_assert(xoffset_world_demo::world_contract_admitted_v);
#if defined(TYPELAYOUT_XOFFSET_PACKED_ENTITY)
    constexpr std::string_view producer_name = "producer_packed";
#else
    constexpr std::string_view producer_name = "producer_ok";
#endif

    boost::typelayout::SigExporter exporter(std::string(producer_name));
    xoffset_world_demo::for_each_contract_type(
        [&]<typename T>(std::string_view key) {
            static_assert(boost::typelayout::is_admitted_v<
                T,
                boost::typelayout::TransferProfile::whole_region_relocation>);
            exporter.add_relocatable<T>(std::string(key));
        });

    const auto output = std::filesystem::path(argv[1]) /
        (std::string(producer_name) + ".sig.hpp");
    std::filesystem::create_directories(output.parent_path());
    return exporter.write(output.string());
}
```

- [ ] **Step 3: Add explicit exporter targets**

Inside the existing Clang/vendor guard in `CMakeLists.txt`, add two `EXCLUDE_FROM_ALL` executables from the same source. Give both the same link/include settings as the demo and define `TYPELAYOUT_XOFFSET_PACKED_ENTITY=1` only for the packed target:

```cmake
add_executable(xoffset_world_export_ok EXCLUDE_FROM_ALL
    example/xoffset_world_demo/export_signatures.cpp)
add_executable(xoffset_world_export_packed EXCLUDE_FROM_ALL
    example/xoffset_world_demo/export_signatures.cpp)
target_compile_definitions(xoffset_world_export_packed PRIVATE
    TYPELAYOUT_XOFFSET_PACKED_ENTITY=1)
```

Do not add a post-build command that writes into the source tree.

- [ ] **Step 4: Build exporters and generate both checked-in fixtures**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-xoffset-demo --target xoffset_world_export_ok xoffset_world_export_packed -j2 && ./build-xoffset-demo/xoffset_world_export_ok example/xoffset_world_demo/sigs && ./build-xoffset-demo/xoffset_world_export_packed example/xoffset_world_demo/sigs'
```

Expected: both files exist, each has four `TypeEntry` rows, and their namespaces are `producer_ok` and `producer_packed` respectively.

- [ ] **Step 5: Implement the local Agreement helper**

For each contract key, search `producer.types[0..type_count)`. Return `incomplete` when a key is absent. Otherwise compare the fixture string and current compile-time signature:

```cpp
template <typename T>
bool entry_matches(const boost::typelayout::TypeEntry& entry) {
    constexpr auto current = boost::typelayout::get_layout_signature<T>();
    return entry.byte_copy_safe &&
        std::string_view(entry.layout_sig) == std::string_view(current);
}
```

Use `for_each_contract_type` to accumulate `saw_missing` and `saw_difference`; missing dominates difference, then return `match`, `differ`, or `incomplete`. Add focused checks proving that the packed fixture differs for the `Entity` key while its other three keys still match.

Expose this focused helper locally:

```cpp
template <typename T>
bool fixture_entry_matches(
    boost::typelayout::PlatformInfo producer,
    std::string_view key);
```

Then make `main` enforce the exact packed pattern:

```cpp
const auto packed_info =
    boost::typelayout::platform::producer_packed::get_platform_info();
if (!fixture_entry_matches<xoffset_world_demo::WorldSnapshot>(
        packed_info, "WorldSnapshot") ||
    fixture_entry_matches<xoffset_world_demo::Entity>(
        packed_info, "Entity") ||
    !fixture_entry_matches<xoffset_world_demo::EntityRelativePtr>(
        packed_info, "EntityRelativePtr") ||
    !fixture_entry_matches<xoffset_world_demo::EntityIndexEntry>(
        packed_info, "EntityIndexEntry")) {
    return 1;
}
```

Print:

```text
Admission[whole_region_relocation]: PASS
Agreement[producer_ok, 4 types]: MATCH
Negative[producer packing ABI drift]: Agreement DIFFER, load skipped
```

- [ ] **Step 6: Build and run the Agreement test**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-xoffset-demo --target xoffset_world_demo -j2 && ctest --test-dir build-xoffset-demo -R "^xoffset_world_demo$" --output-on-failure'
```

Expected: the test passes with normal `MATCH` and packed `DIFFER`.

- [ ] **Step 7: Commit exporter, fixtures, Agreement helper, and build targets**

```powershell
git add -- CMakeLists.txt example/xoffset_world_demo/demo.cpp example/xoffset_world_demo/export_signatures.cpp example/xoffset_world_demo/sigs/producer_ok.sig.hpp example/xoffset_world_demo/sigs/producer_packed.sig.hpp
git commit -m "feat: gate XOffset demo on layout agreement"
```

---

### Task 5: Prove Positive Whole-Region Relocation

**Files:**
- Modify: `example/xoffset_world_demo/demo.cpp`

**Interfaces:**
- Consumes: positive Admission/Agreement gate, `WorldSnapshot`, XOffset verified wire API, and `relative_ptr::raw_delta/get/reset`.
- Produces:
  - `XBuffer build_world()`
  - `void validate_world_graph(XBuffer&)`
  - `std::array<std::int32_t, 7> capture_deltas(XBuffer&)`
  - `std::int32_t party_total_hp(WorldSnapshot&)`
  - `void run_positive_relocation()`

- [ ] **Step 1: Add the positive-flow call before its implementation**

After the normal Admission and Agreement checks in `main`, call:

```cpp
run_positive_relocation();
```

Build the demo. Expected: compilation fails because `run_positive_relocation` is not defined.

- [ ] **Step 2: Build the fixed two-entity checkpoint before linking**

Implement `build_world()` using `XBuffer::create<WorldSnapshot>(4096)` and an `XHandle<WorldSnapshot>`. Perform all potentially allocating operations first:

```text
reserve entities for 2
reserve party for 2
reserve entity_index for 2
emplace 2 entities
emplace 2 party pointer slots
assign names "Hero" and "Boss"
set tick 42, IDs, kinds, positions, and HP 120/300
insert ID -> 0 and ID -> 1
```

Reacquire the root and element addresses through the handle. With no further allocation, call `reset` to establish:

```text
Hero.owner -> null
Boss.owner -> Hero
Hero.target -> Boss
Boss.target -> Hero
party[0] -> Hero
party[1] -> Boss
local_player -> Hero
```

Return the buffer by move. Do not store a region pointer or entity reference across an allocating operation.

- [ ] **Step 3: Implement pre-dereference graph validation**

`validate_world_graph(XBuffer&)` first uses the already-verified XOffset containers to enumerate entity starts as `std::uintptr_t`. For every entity `owner/target`, party entry, and `local_player`, validate the raw integer delta in this order:

```text
region_begin + region_size cannot overflow uintptr_t
zero is accepted as null
signed addition to the anchor cannot overflow uintptr_t
candidate is within [region_begin, region_end)
candidate is aligned for Entity
candidate equals one exact live Entity element start
```

For negative deltas, convert to `std::int64_t` before taking the magnitude so `INT32_MIN` is handled without signed overflow. Do not form an `Entity*` during these checks. After every link passes, verify unique IDs and that `entity_index.find(id)->second` equals each exact entity index. Throw `std::runtime_error` with a specific reason on the first violation.

- [ ] **Step 4: Implement delta capture and validated business assertions**

After validation, `capture_deltas` returns this ordered array:

```text
Hero.owner, Hero.target, Boss.owner, Boss.target,
party[0], party[1], local_player
```

After validation, assert the null/shared/cyclic relationships through `get()`. `party_total_hp` sums the two resolved party members and must return 420 for the initial world.

- [ ] **Step 5: Implement the no-fixup relocation and mutation flow**

`run_positive_relocation()` performs this exact sequence:

```text
build and validate source A
capture pre-save deltas
save_verified<WorldSnapshot>(A)
reacquire and validate A
require A post-save deltas equal pre-save deltas
record A's post-save region base
keep A alive and load_verified<WorldSnapshot> into B
require base(A) != base(B)
validate B before any relative-pointer dereference
require B deltas equal A deltas
verify null/shared/cycle/container relationships
require party_total_hp(B) == 420
increment tick 42 -> 43
find Boss through entity_index and reduce HP 300 -> 250
save_verified<WorldSnapshot>(B)
reacquire and validate B
load_verified<WorldSnapshot> into C
validate C
require tick == 43 and indexed Boss HP == 250
```

Print these result lines:

```text
Relocation: base changed, raw deltas unchanged
Graph: null + shared + cycle + pointer container PASS
Business: party_hp=420, tick=42->43, boss_hp=300->250
Reload: mutation persisted
```

- [ ] **Step 6: Build and run the positive demo**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-xoffset-demo --target xoffset_world_demo -j2 && ./build-xoffset-demo/xoffset_world_demo'
```

Expected: exit code zero and all four positive result lines appear.

- [ ] **Step 7: Commit the positive runtime flow**

```powershell
git add -- example/xoffset_world_demo/demo.cpp
git commit -m "feat: demonstrate no-fixup world relocation"
```

---

### Task 6: Add Native-Pointer and Corrupt-Delta Rejections

**Files:**
- Modify: `example/xoffset_world_demo/demo.cpp`

**Interfaces:**
- Consumes: Task 4's packed Agreement result and Task 5's valid checkpoint/validator.
- Produces: all three required negative outputs and a non-zero test result if any layer accepts what it must reject.

- [ ] **Step 1: Add failing calls for the remaining negative paths**

Replace Task 4's inline packed check/print with a function call, then add these calls after the positive flow:

```cpp
run_native_pointer_negative();
run_packed_agreement_negative();
run_corrupt_delta_negative();
```

Build the demo. Expected: compilation fails because the three functions are not defined.

- [ ] **Step 2: Implement the native-pointer Admission failure**

Define:

```cpp
struct NativePointerEntity {
    std::uint64_t id;
    xoffset_world_demo::Entity* target;
};

static_assert(boost::typelayout::source_context_v<NativePointerEntity> ==
              boost::typelayout::SourceContext::address_space_dependent);
static_assert(!boost::typelayout::is_byte_copy_safe_v<NativePointerEntity>);
static_assert(!boost::typelayout::is_admitted_v<
    NativePointerEntity,
    boost::typelayout::TransferProfile::whole_region_relocation>);
```

`run_native_pointer_negative()` checks the same constant at runtime and prints without constructing bytes or calling an XOffset loader:

```text
Negative[native pointer]: Admission FAIL, load skipped
```

- [ ] **Step 3: Make the packed Agreement skip explicit**

`run_packed_agreement_negative()` obtains `producer_packed::get_platform_info()`, requires `AgreementResult::differ`, and returns after printing:

```text
Negative[producer packing ABI drift]: Agreement DIFFER, load skipped
```

Do not construct or load a packed `WorldSnapshot`.

- [ ] **Step 4: Corrupt one serialized root relative delta**

`run_corrupt_delta_negative()` builds and validates a fresh normal world, calls `save_verified<WorldSnapshot>()`, then reacquires the root. Compute the payload-relative byte offset of `WorldSnapshot::local_player` from `buffer.bytes().data()`. Copy `std::numeric_limits<std::int32_t>::max()` into:

```text
wire.data() + sizeof(XOffsetDatastructure::XWireHeaderV1) + payload_offset
```

Call `XBuffer::load_verified<WorldSnapshot>(wire)` and require that it succeeds. Then call `validate_world_graph` and require a `std::runtime_error` before any application dereference. Print:

```text
Negative[corrupt rel32]: graph REJECT before dereference
```

If XOffset verified load rejects the bytes first, fail the demo because the intended layer separation was not demonstrated.

- [ ] **Step 5: Build, run, and check every required output line**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-xoffset-demo --target xoffset_world_demo -j2 && ./build-xoffset-demo/xoffset_world_demo | tee build-xoffset-demo/xoffset_world_demo.out && grep -F "Admission[whole_region_relocation]: PASS" build-xoffset-demo/xoffset_world_demo.out && grep -F "Agreement[producer_ok, 4 types]: MATCH" build-xoffset-demo/xoffset_world_demo.out && grep -F "Relocation: base changed, raw deltas unchanged" build-xoffset-demo/xoffset_world_demo.out && grep -F "Graph: null + shared + cycle + pointer container PASS" build-xoffset-demo/xoffset_world_demo.out && grep -F "Business: party_hp=420, tick=42->43, boss_hp=300->250" build-xoffset-demo/xoffset_world_demo.out && grep -F "Reload: mutation persisted" build-xoffset-demo/xoffset_world_demo.out && grep -F "Negative[native pointer]: Admission FAIL, load skipped" build-xoffset-demo/xoffset_world_demo.out && grep -F "Negative[producer packing ABI drift]: Agreement DIFFER, load skipped" build-xoffset-demo/xoffset_world_demo.out && grep -F "Negative[corrupt rel32]: graph REJECT before dereference" build-xoffset-demo/xoffset_world_demo.out'
```

Expected: every `grep` succeeds and the command exits zero.

- [ ] **Step 6: Run the focused CTest entries**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && ctest --test-dir build-xoffset-demo -R "^(test_core|xoffset_world_demo)$" --output-on-failure'
```

Expected: both tests pass.

- [ ] **Step 7: Commit the negative cases**

```powershell
git add -- example/xoffset_world_demo/demo.cpp
git commit -m "test: cover XOffset transfer rejection layers"
```

---

### Task 7: Regenerate Evidence, Run the Full Suite, and Review Scope

**Files:**
- Verify: all implementation files from the file map
- Do not modify: `vendor/XOffsetDatastructure/**`

**Interfaces:**
- Consumes: the complete implementation.
- Produces: reproducible fixture evidence and final verification output suitable for the implementation handoff.

- [ ] **Step 1: Rebuild every default target and both exporter targets**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-xoffset-demo -j2 && cmake --build build-xoffset-demo --target xoffset_world_export_ok xoffset_world_export_packed -j2'
```

Expected: all builds succeed.

- [ ] **Step 2: Regenerate fixtures outside the source tree and compare them**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake -E make_directory build-xoffset-demo/fixture-check && ./build-xoffset-demo/xoffset_world_export_ok build-xoffset-demo/fixture-check && ./build-xoffset-demo/xoffset_world_export_packed build-xoffset-demo/fixture-check && diff -I "^// Generated:" example/xoffset_world_demo/sigs/producer_ok.sig.hpp build-xoffset-demo/fixture-check/producer_ok.sig.hpp && diff -I "^// Generated:" example/xoffset_world_demo/sigs/producer_packed.sig.hpp build-xoffset-demo/fixture-check/producer_packed.sig.hpp'
```

Expected: both comparisons succeed; only generation timestamps may differ.

- [ ] **Step 3: Run the full configured CTest suite**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && ctest --test-dir build-xoffset-demo --output-on-failure'
```

Expected: every configured test passes, including the intentional negative-compile and compatibility-negative tests under their existing CTest semantics.

- [ ] **Step 4: Verify representation and dependency boundaries**

```powershell
rg -n "TYPELAYOUT_(REGISTER_OPAQUE|OPAQUE_TYPE)" example/xoffset_world_demo
git -C vendor/XOffsetDatastructure status --short
git -C vendor/XOffsetDatastructure branch --show-current
git -C vendor/XOffsetDatastructure rev-parse HEAD
git diff --check
git diff --cached --check
```

Expected:

- the opaque-registration search has no matches;
- vendor status is empty;
- branch and revision remain `next_cpp26` and `2233004983cd42664e3d6084ec09092b2968ad4e`;
- both diff checks report no whitespace errors.

- [ ] **Step 5: Inspect the final diff for minimality**

```powershell
git diff 3416f1e -- include/boost/typelayout/admission.hpp test/test_core.cpp example/xoffset_world_demo CMakeLists.txt .gitmodules vendor/XOffsetDatastructure
git status --short
```

Confirm that the implementation contains only the approved two-entity model, four evidence keys, positive flow, and three negative cases, and that the user's unrelated deck-document changes are still separate.

- [ ] **Step 6: Request code review and fix only verified findings**

Use `superpowers:requesting-code-review` against the implementation range. Re-run the focused tests after each accepted fix and the full CTest suite after the final fix. Commit any review-driven correction with a focused `fix:` message.

- [ ] **Step 7: Run verification-before-completion**

Use `superpowers:verification-before-completion`, repeat the full build/test and dependency checks it requires, and retain the command output for the final handoff.
