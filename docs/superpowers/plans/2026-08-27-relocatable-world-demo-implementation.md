# Relocatable World Demo Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the XOffset-backed example with the smallest self-contained C++26 game-world checkpoint demo that proves whole-region Admission, four-key Agreement, validated no-fixup relocation, useful read/write behavior, and three layer-specific rejections.

**Architecture:** Keep every stored representation in one fixed 4096-byte region and encode every stored link as a four-byte offset from that region's base. Construction uses builder-issued handles and final-location lifetime start; loading validates a canonical 40-byte envelope, establishes object lifetimes in a staged non-overlapping order, and exposes data only through a validated `RegionView`. TypeLayout's existing profile-aware Admission API remains unchanged; the demo adds only local traits, four stable Agreement keys, and schema-specific validation.

**Tech Stack:** C++26 P2996 static reflection, Boost.TypeLayout headers, `std::start_lifetime_as`, `std::start_lifetime_as_array`, CMake, CTest, GCC 16.2, and Bloomberg Clang P2996.

**Spec:** `docs/superpowers/specs/2026-08-27-relocatable-world-demo-design.md`

**Companion plan:** `docs/superpowers/plans/2026-08-27-relocatable-world-matrix-implementation.md` consumes the completed interfaces and adds locked toolchains plus six-node evidence.

## Global Constraints

- The demo is named `relocatable_world_demo`; do not retain `xoffset_world_demo` or `.xbuf` names in build or runtime paths.
- Do not include, link, initialize, patch, or copy code from XOffsetDatastructure. Keep `vendor/XOffsetDatastructure` pinned at `2233004983cd42664e3d6084ec09092b2968ad4e` only as a non-build reference.
- Do not modify `include/boost/typelayout/admission.hpp` or `test/test_core.cpp` unless a new focused regression proves an actual core defect. The existing `SourceContext`, `TransferProfile`, `source_context_traits`, `region_relocation_traits`, and `is_admitted_v` APIs already satisfy the design.
- Store no native pointer, reference, allocator, iterator, standard container, or process address in the payload.
- `relative_ptr<T>` is exactly one `std::uint32_t offset_plus_one_`; zero is null, and a non-zero value minus one is an offset from the region base.
- Do not use TypeLayout opaque registration for `relative_ptr` or any region container.
- All stored types are standard-layout, trivially copyable, and implicit-lifetime. Dynamic arrays have real array lifetimes before indexing or iteration.
- `RegionBuffer` owns exactly one `alignas(64)` 4096-byte storage allocation and never reallocates.
- The checkpoint envelope is exactly the 40-byte little-endian v1 layout in spec section 9; artifact length is exactly `40 + used_payload_bytes`.
- Only a successfully validated buffer may create a public `RegionView`. Validation uses private byte-level access and starts no lifetime before bounds, alignment, and non-overlap checks pass.
- The four Agreement keys are exactly `WorldSnapshot`, `Entity`, `EntityRelativePtr`, and `EntityIndexEntry`, joined by key rather than registry position.
- The talk executable prints exactly three negative demonstrations: native pointer at Admission, packed `Entity` at Agreement, and corrupt region offset at graph validation. Additional malformed-input tests remain silent CTests.
- `Entity` IDs are 1001 and 2001. Initial state is tick 42, Hero HP 120, Boss HP 300; the mutation is tick 43 and Boss HP 250.
- This is an appendix-only whole-region relocation demo. Do not modify the deck or claim that it replaces Stage 5 `portable_capture` or the required real platform-divergent negative.
- Use path-scoped commits. Never use `git add -A`, never touch the main checkout's presentation artifacts, and never delete the retained vendor submodule.

---

## File Structure

- `example/relocatable_world_demo/region.hpp`: stored pointer/container representations and their TypeLayout traits.
- `example/relocatable_world_demo/region_storage.hpp`: `RegionBuffer`, construction handles, `RegionBuilder`, validated views, checked arithmetic, and map-view access.
- `example/relocatable_world_demo/checkpoint.hpp`: exact envelope constants, typed rejection layer, byte codec declarations, and checkpoint API.
- `example/relocatable_world_demo/checkpoint.cpp`: field-by-field envelope codec and world checkpoint save/load orchestration.
- `example/relocatable_world_demo/world.hpp`: stored world schema, explicit relocation-trait closure, four contract keys, and representation assertions.
- `example/relocatable_world_demo/world_runtime.hpp`: canonical-world construction, validation, queries, mutation checks, and raw-offset capture declarations.
- `example/relocatable_world_demo/world_runtime.cpp`: final-location world construction and staged schema/graph validation.
- `example/relocatable_world_demo/agreement.hpp`: strict four-key fixture lookup and current-build Agreement decisions.
- `example/relocatable_world_demo/export_signatures.cpp`: normal/packed fixture exporter with optional matrix node ID.
- `example/relocatable_world_demo/demo.cpp`: one linear talk-sized positive flow and exactly three visible negatives.
- `example/relocatable_world_demo/sigs/producer_ok.sig.hpp`: generated normal local evidence.
- `example/relocatable_world_demo/sigs/producer_packed.sig.hpp`: generated packed-`Entity` local evidence.
- `test/test_relocatable_region.cpp`: compile-time representation/Admission closure plus builder/view runtime tests.
- `test/test_relocatable_checkpoint.cpp`: exact envelope, staged lifetime, range, overlap, and corruption tests.
- `test/test_relocatable_world.cpp`: canonical graph, index semantics, relocation, mutation, Agreement, and negative-layer tests.
- `CMakeLists.txt`: support library, three focused CTests, demo/export targets, and removal of the old guarded XOffset block.

The additional focused headers and tests are intentional implementation detail beneath the spec's summarized change surface. They prevent the talk executable from becoming a hidden test framework and keep the three visible negative cases exact.

### Task 1: Add Stored Region Representations and Admission Closure

**Files:**
- Create: `example/relocatable_world_demo/region.hpp`
- Create: `test/test_relocatable_region.cpp`
- Modify: `CMakeLists.txt:66-72`

**Interfaces:**
- Consumes: existing `boost::typelayout::{SourceContext, TransferProfile, source_context_traits, region_relocation_traits, is_admitted_v}`.
- Produces:
  - `template<class T> class relocatable_world_demo::region_handle`
  - `template<class T> class relocatable_world_demo::relative_ptr`
  - `relocatable_world_demo::region_string`
  - `template<class T> class relocatable_world_demo::region_vector`
  - `template<class K, class V> struct relocatable_world_demo::region_key_value`
  - `template<class K, class V> class relocatable_world_demo::region_flat_map`

- [ ] **Step 0: Capture the protected-file baseline before any implementation edit**

Run once from the worktree and keep the ignored build-directory manifest through Task 8:

```powershell
New-Item -ItemType Directory -Force build-relocatable-world-baseline | Out-Null
git rev-parse HEAD | Set-Content -NoNewline build-relocatable-world-baseline/commit.txt
if ($LASTEXITCODE -ne 0) { throw "cannot record baseline commit" }
git hash-object docs/talk/cppcon2026-main-deck-content-and-script.md | Set-Content -NoNewline build-relocatable-world-baseline/main-deck.sha
if ($LASTEXITCODE -ne 0) { throw "cannot hash pre-existing main-deck file" }
```

The pre-existing deck file must exist; otherwise stop because its byte baseline cannot be preserved. Its current working-tree bytes may already differ from `HEAD`, so the saved object hash, rather than a clean-tree assumption, protects that user-owned content. The recorded commit anchors tracked deck, vendor, and `.gitmodules` comparisons even after the implementation creates multiple commits.

- [ ] **Step 1: Register the focused test before the header exists**

Add this target immediately after `test_core` in `CMakeLists.txt`:

```cmake
add_executable(test_relocatable_region
    test/test_relocatable_region.cpp)
target_link_libraries(test_relocatable_region PRIVATE typelayout)
target_include_directories(test_relocatable_region PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/example/relocatable_world_demo)
add_test(NAME test_relocatable_region COMMAND test_relocatable_region)
set_tests_properties(test_relocatable_region PROPERTIES
    LABELS "typelayout;example;relocatable-world")
```

Create `test/test_relocatable_region.cpp` with the exact compile-time contract:

```cpp
#include "region.hpp"

#include <boost/typelayout.hpp>

#include <cstdint>
#include <type_traits>

using namespace relocatable_world_demo;
using namespace boost::typelayout;

struct DisabledRegionElement {
    std::uint32_t value;
};

namespace boost::typelayout::v1 {
template <>
struct source_context_traits<::DisabledRegionElement>
    : std::integral_constant<SourceContext, SourceContext::same_region> {};

template <>
struct region_relocation_traits<::DisabledRegionElement> {
    static constexpr bool enabled = false;
};
}

static_assert(sizeof(relative_ptr<std::uint32_t>) == 4);
static_assert(alignof(relative_ptr<std::uint32_t>) == 4);
static_assert(std::is_standard_layout_v<relative_ptr<std::uint32_t>>);
static_assert(std::is_trivially_copyable_v<relative_ptr<std::uint32_t>>);
static_assert(std::is_implicit_lifetime_v<relative_ptr<std::uint32_t>>);
static_assert(!std::is_copy_assignable_v<relative_ptr<std::uint32_t>>);
static_assert(!std::is_move_assignable_v<relative_ptr<std::uint32_t>>);
static_assert(sizeof(region_string) == 8);
static_assert(alignof(region_string) == 4);
static_assert(sizeof(region_vector<std::uint32_t>) == 8);
static_assert(alignof(region_vector<std::uint32_t>) == 4);
static_assert(sizeof(region_flat_map<std::uint64_t, std::uint32_t>) == 8);
static_assert(alignof(region_flat_map<std::uint64_t, std::uint32_t>) == 4);
static_assert(std::is_standard_layout_v<region_string>);
static_assert(std::is_trivially_copyable_v<region_string>);
static_assert(std::is_implicit_lifetime_v<region_string>);
static_assert(!std::is_copy_assignable_v<region_string>);
static_assert(!std::is_move_assignable_v<region_string>);
static_assert(std::is_standard_layout_v<region_vector<std::uint32_t>>);
static_assert(std::is_trivially_copyable_v<region_vector<std::uint32_t>>);
static_assert(std::is_implicit_lifetime_v<region_vector<std::uint32_t>>);
static_assert(!std::is_copy_assignable_v<region_vector<std::uint32_t>>);
static_assert(!std::is_move_assignable_v<region_vector<std::uint32_t>>);
static_assert(std::is_standard_layout_v<
    region_flat_map<std::uint64_t, std::uint32_t>>);
static_assert(std::is_trivially_copyable_v<
    region_flat_map<std::uint64_t, std::uint32_t>>);
static_assert(std::is_implicit_lifetime_v<
    region_flat_map<std::uint64_t, std::uint32_t>>);
static_assert(!std::is_copy_assignable_v<
    region_flat_map<std::uint64_t, std::uint32_t>>);
static_assert(!std::is_move_assignable_v<
    region_flat_map<std::uint64_t, std::uint32_t>>);
static_assert(source_context_v<relative_ptr<std::uint32_t>> ==
              SourceContext::same_region);
static_assert(!is_admitted_v<relative_ptr<std::uint32_t>,
    TransferProfile::ordinary_copy>);
static_assert(is_admitted_v<relative_ptr<std::uint32_t>,
    TransferProfile::whole_region_relocation>);
static_assert(!is_admitted_v<region_string,
    TransferProfile::ordinary_copy>);
static_assert(!is_admitted_v<region_vector<std::uint32_t>,
    TransferProfile::ordinary_copy>);
static_assert(!is_admitted_v<
    region_flat_map<std::uint64_t, std::uint32_t>,
    TransferProfile::ordinary_copy>);
static_assert(!is_admitted_v<region_vector<DisabledRegionElement>,
    TransferProfile::whole_region_relocation>);

int main() {}
```

- [ ] **Step 2: Run the focused build and verify the missing-header failure**

Configure a fresh P2996 build inside WSL:

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && cmake -S . -B build-relocatable-world -G Ninja -DCMAKE_CXX_COMPILER=/root/clang-p2996-install/bin/clang++ -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++" -DTYPELAYOUT_BUILD_COMPAT_CI=OFF'
```

Run:

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_relocatable_region -j2'
```

Expected: FAIL because `region.hpp` does not exist.

- [ ] **Step 3: Implement only the stored representations**

Create `region.hpp` with private stored fields and these exact public observations:

```cpp
namespace relocatable_world_demo {

class RegionBuilder;
class RegionView;
class WorldRegionValidator;
struct WorldRegionAccess;
template <typename T> class relative_ptr;

template <typename T>
class region_handle {
public:
    constexpr region_handle() noexcept = default;
    constexpr bool is_null() const noexcept { return offset_plus_one_ == 0; }
    constexpr std::uint32_t raw_offset_plus_one() const noexcept {
        return offset_plus_one_;
    }

private:
    constexpr region_handle(const RegionBuilder* owner,
                            std::uint32_t value) noexcept;
    const RegionBuilder* owner_ = nullptr;
    std::uint32_t offset_plus_one_ = 0;
    friend class RegionBuilder;
    friend class WorldRegionValidator;
};

template <typename T>
class relative_ptr {
public:
    constexpr relative_ptr() noexcept = default;
    constexpr relative_ptr(const relative_ptr&) noexcept = default;
    constexpr bool is_null() const noexcept { return offset_plus_one_ == 0; }
    constexpr explicit operator bool() const noexcept { return !is_null(); }
    constexpr std::uint32_t raw_offset_plus_one() const noexcept {
        return offset_plus_one_;
    }
private:
    constexpr relative_ptr& operator=(const relative_ptr&) noexcept = default;
    constexpr relative_ptr& operator=(relative_ptr&&) noexcept = default;
    constexpr void reset_unchecked(region_handle<T> target) noexcept {
        offset_plus_one_ = target.raw_offset_plus_one();
    }
    std::uint32_t offset_plus_one_ = 0;
    friend class RegionBuilder;
};

class region_string {
public:
    constexpr region_string() noexcept = default;
    constexpr region_string(const region_string&) noexcept = default;
    constexpr std::uint32_t size() const noexcept { return size_; }

private:
    constexpr region_string& operator=(const region_string&) noexcept = default;
    constexpr region_string& operator=(region_string&&) noexcept = default;
    relative_ptr<char> data_{};
    std::uint32_t size_ = 0;
    friend class RegionBuilder;
    friend class RegionView;
    friend class WorldRegionValidator;
};

template <typename T>
class region_vector {
public:
    constexpr region_vector() noexcept = default;
    constexpr region_vector(const region_vector&) noexcept = default;
    constexpr std::uint32_t size() const noexcept { return size_; }

private:
    constexpr region_vector& operator=(const region_vector&) noexcept = default;
    constexpr region_vector& operator=(region_vector&&) noexcept = default;
    relative_ptr<T> data_{};
    std::uint32_t size_ = 0;
    friend class RegionBuilder;
    friend class RegionView;
    friend class WorldRegionValidator;
};

template <typename K, typename V>
struct region_key_value {
    K key;
    V value;
};

template <typename K, typename V>
class region_flat_map {
public:
    constexpr region_flat_map() noexcept = default;
    constexpr region_flat_map(const region_flat_map&) noexcept = default;
    constexpr std::uint32_t size() const noexcept { return entries_.size(); }

private:
    constexpr region_flat_map& operator=(const region_flat_map&) noexcept = default;
    constexpr region_flat_map& operator=(region_flat_map&&) noexcept = default;
    region_vector<region_key_value<K, V>> entries_{};
    friend class RegionBuilder;
    friend class RegionView;
    friend class WorldRegionValidator;
};

} // namespace relocatable_world_demo
```

Add demo-local TypeLayout partial specializations in `boost::typelayout::v1`:

```cpp
template <typename T>
struct source_context_traits<relocatable_world_demo::relative_ptr<T>>
    : std::integral_constant<SourceContext, SourceContext::same_region> {};

template <>
struct source_context_traits<relocatable_world_demo::region_string>
    : std::integral_constant<SourceContext, SourceContext::same_region> {};

template <typename T>
struct source_context_traits<relocatable_world_demo::region_vector<T>>
    : std::integral_constant<SourceContext,
          join_source_context(SourceContext::same_region,
                              source_context_v<T>)> {};

template <>
struct region_relocation_traits<relocatable_world_demo::region_string> {
    static constexpr bool enabled = is_admitted_v<char,
        TransferProfile::whole_region_relocation>;
};

template <typename T>
struct region_relocation_traits<relocatable_world_demo::region_vector<T>> {
    static constexpr bool enabled = is_admitted_v<T,
        TransferProfile::whole_region_relocation>;
};

template <typename K, typename V>
struct region_relocation_traits<
    relocatable_world_demo::region_key_value<K, V>> {
    static constexpr bool enabled =
        is_admitted_v<K, TransferProfile::whole_region_relocation> &&
        is_admitted_v<V, TransferProfile::whole_region_relocation>;
};

template <typename K, typename V>
struct region_relocation_traits<
    relocatable_world_demo::region_flat_map<K, V>> {
    static constexpr bool enabled = is_admitted_v<
        relocatable_world_demo::region_key_value<K, V>,
        TransferProfile::whole_region_relocation>;
};
```

`region_handle<T>` is construction-only and may therefore carry a native pointer to its issuing `RegionBuilder` in addition to its checked offset. It is never part of the payload or an Agreement key. The only operation that calls private `relative_ptr<T>::reset_unchecked()` is `RegionBuilder::bind(destination, target)`: it requires the builder to remain active, the destination object to lie wholly inside that builder's storage, and every non-null target handle to have `owner_ == this`. A stack destination, a cross-builder handle, or any bind after `finish()` throws `std::invalid_argument` or `std::logic_error` without adding a base address to the stored four-byte representation.

Keep descriptor copy construction public and trivial, but default copy/move assignment privately. Existing trusted view/validator friends never assign descriptors; only `RegionBuilder` writes them. This makes `std::is_copy_assignable_v` and `std::is_move_assignable_v` false to callers while preserving trivial copyability; the containing `Entity` and `WorldSnapshot` assignments are consequently deleted. Add compile-time trait/`requires` assertions for every descriptor plus the two schema types, proving that neither direct cross-builder descriptor assignment nor whole-object assignment can bypass `bind()`.

Do not add `get()`, a stored region base, capacity, allocator, or mutable topology API to any payload type.

- [ ] **Step 4: Run the focused test**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_relocatable_region -j2 && ctest --test-dir build-relocatable-world -R "^test_relocatable_region$" --output-on-failure'
```

Expected: PASS.

- [ ] **Step 5: Commit the representation boundary**

```bash
git add -- CMakeLists.txt example/relocatable_world_demo/region.hpp test/test_relocatable_region.cpp
git commit -m "feat: add self-contained relocatable region types"
```

### Task 2: Add One-Allocation Storage, Builder Handles, and Validated Views

**Files:**
- Create: `example/relocatable_world_demo/region_storage.hpp`
- Modify: `test/test_relocatable_region.cpp`

**Interfaces:**
- Consumes: Task 1's descriptors and `region_handle<T>`.
- Produces:
  - `inline constexpr std::size_t region_capacity = 4096`
  - `template<class T> class region_array_handle`
  - `class RegionBuffer`
  - `class RegionBuilder`
  - `class RegionView`
  - `template<class K, class V, class Entry> class basic_region_flat_map_view`
  - internal `detail::checked_add`, `detail::checked_multiply`, and `detail::checked_align_up`

- [ ] **Step 1: Add failing builder and view tests**

Extend `test_relocatable_region.cpp` with a stored fixture and Release-active checks:

```cpp
#include "region_storage.hpp"

#include <string_view>

struct RegionFixture {
    region_string name;
    region_vector<std::uint32_t> values;
    relative_ptr<std::uint32_t> selected;
};

void test_builder_and_view() {
    RegionBuilder builder;
    const auto root = builder.make_object<WorldSnapshot>();
    const auto fixture = builder.make_object<RegionFixture>();
    const auto values = builder.make_array<std::uint32_t>(3);
    builder.set(values, 0, std::uint32_t{7});
    builder.set(values, 1, std::uint32_t{11});
    builder.set(values, 2, std::uint32_t{13});
    builder.bind(fixture, &RegionFixture::values, values);
    builder.assign(fixture, &RegionFixture::name, "Hero");
    builder.bind(fixture, &RegionFixture::selected,
                 builder.element_handle(values, 1));

    auto buffer = std::move(builder).finish(root);
    require(!buffer.is_validated());
    require_throws<std::logic_error>([&] { buffer.view(); });
    // Inspect descriptor and element representations through used_bytes().
}
```

Also add tests that `make_array<T>(0)` binds null/zero, a 4097-byte request throws `std::length_error`, a maximal public array count is rejected as over-capacity before cursor movement, and a second allocation never changes the storage base. Exercise the three checked arithmetic helpers directly with `std::numeric_limits<std::size_t>::max()` to prove multiplication, addition, and align-up overflow rejection; the public `std::uint32_t` counts cannot overflow 64-bit `std::size_t` on the declared nodes. Call all tests from `main()`.

Create a second builder and prove that handle/member `bind`, `assign`, and `set` reject foreign destination or source handles. Add Release-active negative tests proving that null pointer-to-members are rejected before either an ordinary write or topology bind can dereference them. Prove that invalid topology sources leave every already-bound destination byte unchanged. Prove at compile time that direct stack-destination bind/assign calls are not expressible, mutable `get`/`at` do not exist, ordinary `set` rejects descriptors, native pointers, converting value types, and non-trivial assignment expressions, and `finish` rejects every root handle type except `WorldSnapshot`. Include a runtime RED witness in which an old converting `set` finalizes and validates mid-call before resuming its write; the final concept/API test must make that expression uncallable. Retain valid handles across `finish()` and prove every subsequent builder operation throws `std::logic_error`. Add compile-time checks that `RegionFixture`, every descriptor, and later `Entity`/`WorldSnapshot` are not publicly copy- or move-assignable. These are construction-boundary tests, not payload state.

Add `static_assert` checks that `RegionBuilder` is neither copy- nor move-constructible/assignable. Its address is the construction-handle provenance token, so moving it while handles exist is forbidden by design.

- [ ] **Step 2: Run and verify the missing-storage-header failure**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_relocatable_region -j2'
```

Expected: FAIL because `region_storage.hpp` does not exist.

- [ ] **Step 3: Implement checked storage and construction handles**

Use one owning allocation outside the payload:

```cpp
inline constexpr std::size_t region_capacity = 4096;

struct alignas(64) RegionStorage {
    std::byte bytes[region_capacity]{};
};

class RegionBuffer {
public:
    RegionBuffer();
    RegionBuffer(RegionBuffer&& other) noexcept;
    RegionBuffer& operator=(RegionBuffer&& other) noexcept;
    RegionBuffer(const RegionBuffer&) = delete;
    RegionBuffer& operator=(const RegionBuffer&) = delete;

    bool is_validated() const noexcept;
    std::span<const std::byte> used_bytes() const noexcept;
    RegionView view() const;

private:
    enum class state {
        building,
        constructed_unvalidated,
        copied_bytes_unvalidated,
        validated
    };
    std::unique_ptr<RegionStorage> storage_;
    std::uint32_t used_bytes_ = 0;
    std::uint32_t root_offset_ = 0;
    state state_ = state::building;
    friend class RegionBuilder;
    friend class WorldRegionValidator;
    friend struct WorldRegionAccess;
    friend RegionBuffer load_checkpoint(std::span<const std::byte>);
};
```

`RegionBuilder` owns a `RegionBuffer` and exposes only checked final-location construction:

```cpp
class RegionBuilder {
public:
    RegionBuilder();
    RegionBuilder(const RegionBuilder&) = delete;
    RegionBuilder& operator=(const RegionBuilder&) = delete;
    RegionBuilder(RegionBuilder&&) = delete;
    RegionBuilder& operator=(RegionBuilder&&) = delete;

    template <typename T>
    region_handle<T> make_object();

    template <typename T>
    region_array_handle<T> make_array(std::uint32_t count);

    template <typename Owner, typename Member, typename Value>
        requires ordinary_copy_admitted<Member> &&
                 std::is_same_v<std::remove_cvref_t<Value>,
                                std::remove_cv_t<Member>> &&
                 std::is_trivially_assignable_v<Member&, Value&&>
    void set(region_handle<Owner> destination,
             Member Owner::* member,
             Value&& value);

    template <typename Owner, typename Member, typename Value>
        requires ordinary_copy_admitted<Member> &&
                 std::is_same_v<std::remove_cvref_t<Value>,
                                std::remove_cv_t<Member>> &&
                 std::is_trivially_assignable_v<Member&, Value&&>
    void set(region_array_handle<Owner> destination,
             std::uint32_t index,
             Member Owner::* member,
             Value&& value);

    template <typename T, typename Value>
        requires ordinary_copy_admitted<T> &&
                 std::is_same_v<std::remove_cvref_t<Value>,
                                std::remove_cv_t<T>> &&
                 std::is_trivially_assignable_v<T&, Value&&>
    void set(region_array_handle<T> destination,
             std::uint32_t index,
             Value&& value);

    template <typename T>
    region_handle<T> element_handle(
        region_array_handle<T> handle,
        std::uint32_t index) const;

    // Dedicated object and array-element handle/member overload families
    // exist for region_vector, region_flat_map, and relative_ptr.
    template <typename Owner, typename T>
    void bind(region_handle<Owner>,
              region_vector<T> Owner::*,
              region_array_handle<T>);

    template <typename T>
    void bind(region_array_handle<relative_ptr<T>>,
              std::uint32_t index,
              region_handle<T>);

    template <typename Owner>
    void assign(region_handle<Owner>,
                region_string Owner::*,
                std::string_view);

    // Matching array-element member overloads carry a checked index; map and
    // relative-pointer members follow the same handle/member form.

    RegionBuffer finish(region_handle<WorldSnapshot> root) &&;
};
```

`region_array_handle<T>` carries the same issuing-builder provenance plus checked first-element offset and count; it is construction-only and never stored. No public builder operation returns a mutable typed reference, pointer, or span. Ordinary `set` accepts only a member or whole array element admitted for `ordinary_copy`, explicitly excluding native pointers and region descriptors; its value must have the exact cvref-stripped target type and the selected assignment must be trivial, so conversion or custom assignment cannot reenter the builder. Dedicated `bind`/`assign` overloads take a destination handle, optional checked array index, and pointer-to-member; they reject a null pointer-to-member and complete every active-state, destination provenance/index, and source provenance/null/count check before typed destination resolution. They return `void`, so stack destinations are not expressible. `finish()` accepts only a null-or-owned `region_handle<WorldSnapshot>`, rejects null or foreign handles, records that exact root offset and checked cursor, and permanently closes the builder. Implement allocation with checked `align_up`, checked multiplication, and checked cursor addition. Call `std::start_lifetime_as<T>` for a single object and `std::start_lifetime_as_array<T>` once for a non-empty array. Zero storage first, populate objects only at their final addresses, and reject every operation after `finish()`.

Implement both `RegionBuffer` move operations explicitly: transfer storage and metadata, then reset the source to an empty non-validated state. `used_bytes()` on a moved-from buffer returns an empty span, while `view()` and the later schema-bound root accessor reject it. Extend the runtime test to move a finished buffer once and check both the preserved destination base and the inert source.

- [ ] **Step 4: Implement access only through a validated region view**

`RegionView` derives all addresses by byte offset from one buffer base. It is constructible only by `RegionBuffer::view()` after `state_ == state::validated`:

```cpp
class RegionView {
public:
    template <typename T>
    const T* resolve(const relative_ptr<T>& pointer) const;

    template <typename T>
    std::span<const T> elements(const region_vector<T>& vector) const;

    std::string_view text(const region_string& string) const;

    template <typename K, typename V>
    basic_region_flat_map_view<K, V, const region_key_value<K, V>>
    map(const region_flat_map<K, V>& value) const;
};
```

Before using a descriptor, every method checks that the complete descriptor object lies inside the view's own used payload; passing a stack descriptor or one from another validated buffer throws `std::invalid_argument`. Implement containment with byte-object bounds plus `std::less<const std::byte*>` total ordering, never subtraction or relational operators between unrelated pointers and never pointer-to-integer round-trips. The map view implements `begin`, `end`, and binary-search `find`; it never mutates topology. These functions do not repeat target range/graph validation: successful whole-buffer validation plus descriptor identity is the precondition.

- [ ] **Step 5: Run builder/view tests**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_relocatable_region -j2 && ctest --test-dir build-relocatable-world -R "^test_relocatable_region$" --output-on-failure'
```

Expected: PASS, including alignment, overflow, empty/non-empty, non-reallocation, raw relative encoding, compile-time rejection of wrong root types and ordinary descriptor/native-pointer writes, active rejection of null pointer-to-members, absence of mutable `get`/`at`, and rejection of typed access before validation. Positive `RegionView` access is deliberately deferred to Task 5, where the schema validator can establish the required trust boundary without a test-only validation bypass.

- [ ] **Step 6: Commit storage and view behavior**

```bash
git add -- example/relocatable_world_demo/region_storage.hpp test/test_relocatable_region.cpp
git commit -m "feat: add fixed relocatable region storage"
```

### Task 3: Add the World Schema and Four-Type Contract

**Files:**
- Create: `example/relocatable_world_demo/world.hpp`
- Create: `example/relocatable_world_demo/world_runtime.hpp`
- Create: `example/relocatable_world_demo/world_runtime.cpp`
- Create: `test/test_relocatable_world.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 1-2 region representations and builder.
- Produces:
  - `Position`, `EntityKind`, `Entity`, `EntityIndexEntry`, `WorldSnapshot`
  - `inline constexpr bool world_contract_admitted_v`
  - `template<class F> constexpr void for_each_contract_type(F&&)`
  - `region_handle<WorldSnapshot> populate_canonical_world(RegionBuilder&)`
  - constants `hero_id == 1001` and `boss_id == 2001`

- [ ] **Step 1: Add the failing schema/Admission test**

Create `test/test_relocatable_world.cpp`:

```cpp
#include "world.hpp"
#include "world_runtime.hpp"

#include <boost/typelayout.hpp>

#include <cassert>
#include <type_traits>

using namespace relocatable_world_demo;

template <typename T>
inline constexpr bool stored_type_contract_v =
    std::is_standard_layout_v<T> &&
    std::is_trivially_copyable_v<T> &&
    std::is_implicit_lifetime_v<T> &&
    alignof(T) <= 64;

static_assert(world_contract_admitted_v);
static_assert(stored_type_contract_v<char>);
static_assert(stored_type_contract_v<Position>);
static_assert(stored_type_contract_v<EntityKind>);
static_assert(stored_type_contract_v<region_string>);
static_assert(stored_type_contract_v<EntityRelativePtr>);
static_assert(stored_type_contract_v<Entity>);
static_assert(stored_type_contract_v<EntityIndexEntry>);
static_assert(stored_type_contract_v<region_vector<Entity>>);
static_assert(stored_type_contract_v<
    region_flat_map<std::uint64_t, std::uint32_t>>);
static_assert(stored_type_contract_v<region_vector<EntityRelativePtr>>);
static_assert(stored_type_contract_v<WorldSnapshot>);
static_assert(sizeof(EntityRelativePtr) == 4);
static_assert(alignof(EntityRelativePtr) == 4);
static_assert(sizeof(region_string) == 8 && alignof(region_string) == 4);
static_assert(sizeof(region_vector<Entity>) == 8 &&
              alignof(region_vector<Entity>) == 4);
static_assert(sizeof(region_flat_map<std::uint64_t, std::uint32_t>) == 8 &&
              alignof(region_flat_map<std::uint64_t, std::uint32_t>) == 4);
static_assert(sizeof(region_vector<EntityRelativePtr>) == 8 &&
              alignof(region_vector<EntityRelativePtr>) == 4);
static_assert(!boost::typelayout::get_layout_signature<EntityRelativePtr>()
    .contains(boost::typelayout::FixedString{"O("}));

int main() {
    RegionBuilder builder;
    const auto root = populate_canonical_world(builder);
    auto buffer = std::move(builder).finish(root);
    require(!buffer.is_validated());
    require(buffer.used_bytes().size() <= region_capacity);
    // Read the trivially-copyable representation from used_bytes() to verify
    // tick, descriptors, entities, names, index entries, and graph offsets;
    // no mutable construction reference is exposed.
}
```

Register `relocatable_world_support` and the test:

```cmake
add_library(relocatable_world_support STATIC
    example/relocatable_world_demo/world_runtime.cpp)
target_link_libraries(relocatable_world_support PUBLIC typelayout)
target_include_directories(relocatable_world_support PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/example/relocatable_world_demo)

add_executable(test_relocatable_world test/test_relocatable_world.cpp)
target_link_libraries(test_relocatable_world PRIVATE relocatable_world_support)
add_test(NAME test_relocatable_world COMMAND test_relocatable_world)
set_tests_properties(test_relocatable_world PROPERTIES
    LABELS "typelayout;example;relocatable-world")
```

- [ ] **Step 2: Run and verify the missing-schema failure**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_relocatable_world -j2'
```

Expected: FAIL because `world.hpp` and `world_runtime.cpp` do not exist.

- [ ] **Step 3: Implement the exact stored world model**

Use the declarations from the spec, with packing applied only to `Entity`:

```cpp
struct Position {
    std::int32_t x;
    std::int32_t y;
};

enum class EntityKind : std::uint8_t { player, boss };

#if defined(TYPELAYOUT_RELOCATABLE_WORLD_PACKED_ENTITY)
#pragma pack(push, 1)
#endif
struct Entity {
    std::uint64_t id;
    EntityKind kind;
    Position position;
    std::int32_t hp;
    region_string name;
    relative_ptr<Entity> owner;
    relative_ptr<Entity> target;
};
#if defined(TYPELAYOUT_RELOCATABLE_WORLD_PACKED_ENTITY)
#pragma pack(pop)
#endif

using EntityIndexEntry = region_key_value<std::uint64_t, std::uint32_t>;
using EntityRelativePtr = relative_ptr<Entity>;

struct WorldSnapshot {
    std::uint64_t tick;
    region_vector<Entity> entities;
    region_flat_map<std::uint64_t, std::uint32_t> entity_index;
    region_vector<EntityRelativePtr> party;
    EntityRelativePtr local_player;
};
```

- [ ] **Step 4: Add explicit relocation-trait closure and stable keys**

Specialize `region_relocation_traits<Entity>` and `region_relocation_traits<WorldSnapshot>` as the exact conjunctions in spec section 10. Do not specialize either to unconditional `true`.

Define the stable registry:

```cpp
template <typename F>
constexpr void for_each_contract_type(F&& fn) {
    fn.template operator()<WorldSnapshot>("WorldSnapshot");
    fn.template operator()<Entity>("Entity");
    fn.template operator()<EntityRelativePtr>("EntityRelativePtr");
    fn.template operator()<EntityIndexEntry>("EntityIndexEntry");
}

inline constexpr bool world_contract_admitted_v =
    boost::typelayout::is_admitted_v<WorldSnapshot, whole_region_profile> &&
    boost::typelayout::is_admitted_v<Entity, whole_region_profile> &&
    boost::typelayout::is_admitted_v<EntityRelativePtr, whole_region_profile> &&
    boost::typelayout::is_admitted_v<EntityIndexEntry, whole_region_profile>;
```

Here `whole_region_profile` is a local `inline constexpr auto` equal to `TransferProfile::whole_region_relocation`.

- [ ] **Step 5: Populate the canonical graph only in final storage**

Implement `populate_canonical_world()` with this strict sequence:

```text
make WorldSnapshot object
make Entity[2], EntityIndexEntry[2], EntityRelativePtr[2]
bind the three root descriptors by root handle plus pointer-to-member
populate Hero and Boss scalar members through Admission-constrained array/member set operations
assign Hero and Boss names through array handle, index, and pointer-to-member
populate sorted index entries {1001,0}, {2001,1} through whole-element ordinary set
obtain entity element handles after every allocation is complete
bind owner/target/party/local_player from those handles without exposing a mutable reference
return the root handle to the caller
```

The graph values are exactly those in spec section 5. Do not construct or copy a linked temporary `Entity` or `EntityRelativePtr`. This task deliberately stops before `finish()`: Task 5 will finish the builder and expose `build_canonical_world()` only after it can run the real schema validator.

- [ ] **Step 6: Run schema and canonical-builder tests**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_core test_relocatable_region test_relocatable_world -j2 && ctest --test-dir build-relocatable-world -R "^(test_core|test_relocatable_region|test_relocatable_world)$" --output-on-failure'
```

Expected: PASS.

- [ ] **Step 7: Commit the schema contract**

```bash
git add -- CMakeLists.txt example/relocatable_world_demo/world.hpp example/relocatable_world_demo/world_runtime.hpp example/relocatable_world_demo/world_runtime.cpp test/test_relocatable_world.cpp
git commit -m "feat: add relocatable world contract"
```

### Task 4: Add the Exact 40-Byte Checkpoint Envelope

**Files:**
- Create: `example/relocatable_world_demo/checkpoint.hpp`
- Create: `example/relocatable_world_demo/checkpoint.cpp`
- Create: `test/test_relocatable_checkpoint.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: a byte payload, root offset, and fixed capacity.
- Produces:
  - `enum class rejection_layer { envelope, region, graph }`
  - `class checkpoint_error : public std::runtime_error`
  - `inline constexpr std::size_t checkpoint_header_size = 40`
  - `std::vector<std::byte> encode_checkpoint(std::span<const std::byte>, std::uint32_t)`
  - `decoded_checkpoint decode_checkpoint_envelope(std::span<const std::byte>)`

- [ ] **Step 1: Write exact byte-codec tests**

Create `test/test_relocatable_checkpoint.cpp` with helpers that read and write little-endian `u16/u32`. Use a 64-byte synthetic payload and root offset zero so the envelope codec has no dependency on schema validation, then assert:

```cpp
std::array<std::byte, 64> payload{};
auto bytes = encode_checkpoint(payload, 0);
assert(bytes.size() == checkpoint_header_size + payload.size());
assert(std::memcmp(bytes.data(), "TLWORLD\0", 8) == 0);
assert(read_u16_le(bytes, 8) == 1);
assert(read_u16_le(bytes, 10) == 40);
assert(std::memcmp(bytes.data() + 12, "64LE", 4) == 0);
assert(read_u32_le(bytes, 16) == payload.size());
assert(read_u32_le(bytes, 20) == 0);
assert(read_u32_le(bytes, 24) == 0);
assert(read_u32_le(bytes, 28) == 0);
assert(std::memcmp(bytes.data() + 32, "WORLDV1\0", 8) == 0);
```

For each of magic, version, header size, format tag, flags, reserved field, schema tag, truncated bytes, trailing bytes, payload size 4097, and root offset not smaller than the encoded used payload size, mutate one copy and assert `decode_checkpoint_envelope()` throws `checkpoint_error` with `layer() == rejection_layer::envelope`. Root alignment and complete `WorldSnapshot` extent belong to the region layer and are tested in Task 5.

- [ ] **Step 2: Register and run the failing checkpoint test**

Add `checkpoint.cpp` to `relocatable_world_support`, add the test target, and run:

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_relocatable_checkpoint -j2'
```

Expected: FAIL because the checkpoint files do not exist.

- [ ] **Step 3: Implement field-by-field envelope encoding**

Use these constants and no native header struct copy:

```cpp
inline constexpr std::array<std::byte, 8> checkpoint_magic = {
    std::byte{'T'}, std::byte{'L'}, std::byte{'W'}, std::byte{'O'},
    std::byte{'R'}, std::byte{'L'}, std::byte{'D'}, std::byte{0}};
inline constexpr std::array<std::byte, 4> checkpoint_format = {
    std::byte{'6'}, std::byte{'4'}, std::byte{'L'}, std::byte{'E'}};
inline constexpr std::array<std::byte, 8> checkpoint_schema = {
    std::byte{'W'}, std::byte{'O'}, std::byte{'R'}, std::byte{'L'},
    std::byte{'D'}, std::byte{'V'}, std::byte{'1'}, std::byte{0}};
```

Encode and decode offsets 0, 8, 10, 12, 16, 20, 24, 28, and 32 exactly as the spec table states. Check every integer conversion and require exact artifact length before allocating or copying payload bytes.

- [ ] **Step 4: Keep the codec independent of object lifetimes**

Do not add a payload checksum. `encode_checkpoint()` writes the 40-byte envelope followed by exactly the supplied payload; `decode_checkpoint_envelope()` returns only a checked byte span plus the encoded root offset and starts no typed lifetime. Task 5 composes this codec with `RegionBuffer` and distinguishes region and graph failures.

- [ ] **Step 5: Run the exact envelope tests**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_relocatable_checkpoint -j2 && ctest --test-dir build-relocatable-world -R "^test_relocatable_checkpoint$" --output-on-failure'
```

Expected: PASS for the valid envelope and every exact envelope rejection.

- [ ] **Step 6: Commit the envelope**

```bash
git add -- CMakeLists.txt example/relocatable_world_demo/checkpoint.hpp example/relocatable_world_demo/checkpoint.cpp test/test_relocatable_checkpoint.cpp
git commit -m "feat: add canonical world checkpoint envelope"
```

### Task 5: Implement Staged Lifetime, Range, Index, and Graph Validation

**Files:**
- Modify: `example/relocatable_world_demo/region_storage.hpp`
- Modify: `example/relocatable_world_demo/world_runtime.hpp`
- Modify: `example/relocatable_world_demo/world_runtime.cpp`
- Modify: `example/relocatable_world_demo/checkpoint.hpp`
- Modify: `example/relocatable_world_demo/checkpoint.cpp`
- Modify: `test/test_relocatable_checkpoint.cpp`
- Modify: `test/test_relocatable_world.cpp`

**Interfaces:**
- Consumes: Task 4 envelope-decoded payload and Task 3 schema.
- Produces:
  - `void validate_and_freeze_world(RegionBuffer&)`
  - `RegionBuffer build_canonical_world()`
  - `const WorldSnapshot& world_root(const RegionBuffer&)`
  - `std::vector<std::byte> save_checkpoint(const RegionBuffer&)`
  - `RegionBuffer load_checkpoint(std::span<const std::byte>)`

- [ ] **Step 1: Add focused malformed-region tests**

First add the four new public declarations from this task to `world_runtime.hpp` and `checkpoint.hpp`, without implementations. Add a helper that starts from `save_checkpoint(build_canonical_world())`, modifies one little-endian payload field, and asserts the exact rejection layer. Cover these region failures individually:

```text
root misalignment or incomplete extent
entities null with nonzero count and non-null with zero count
entities misaligned
entities out of bounds
encoded entity extent larger than the used payload/capacity
index entries null with nonzero count and non-null with zero count
index entries misaligned
index-entry extent larger than the used payload/capacity
party null with nonzero count and non-null with zero count
party range misaligned
party range out of bounds
party extent larger than the used payload/capacity
one name null with nonzero size and non-null with zero size
one name out of bounds
one name offset-plus-size larger than the used payload/capacity
one name overlapping the root
one name overlapping the entity array
the index and party ranges overlapping
unsorted or duplicate index keys
index size different from entity count
duplicate entity IDs
index value outside the entity array
entry key different from entities[value].id
one entity missing from index coverage
```

Add graph-layer cases for a non-null offset outside the region, a misaligned entity offset, and an offset into the middle of an entity. Only the out-of-region `local_player` case will later be printed by the demo. The incomplete-root fixture uses the largest aligned offset below the payload end and asserts the extent-specific rejection. Because equal index/entity counts make duplicate and missing values the same failed one-to-one invariant, the second-value-to-zero fixture asserts the reachable missing-coverage reason before key/ID agreement is checked; there is no separate unreachable duplicate-value diagnostic.

Also add the first positive typed-access tests here. A default root with null/zero entity, index, and party descriptors validates as an empty world. A one-entity fixture with an empty name validates the empty-string rule while exercising non-empty vector/map ranges; explicitly assert that character payload alignment is vacuous because `alignof(char) == 1`. Finally, call `build_canonical_world()`, require `is_validated()`, then use only `world_root(buffer)` and its bound read-only `RegionView` to check both non-empty names, all three stored ranges, selected relationships, and binary-search lookup. Pass the view a descriptor copied to the stack and one belonging to a second validated buffer; both must throw before offset resolution. Retain every construction handle through finish and validation, attempt every surviving builder operation category, and require `std::logic_error`, byte-for-byte unchanged payload, and an unchanged valid typed world. These cases replace the deliberately unavailable positive view test from Task 2.

- [ ] **Step 2: Run and verify malformed cases are not yet rejected correctly**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_relocatable_checkpoint test_relocatable_world -j2 && ctest --test-dir build-relocatable-world -R "^(test_relocatable_checkpoint|test_relocatable_world)$" --output-on-failure'
```

Expected: FAIL first because the newly declared build/save/load/validation functions are undefined. After adding only enough orchestration to link, the malformed cases must still fail until the staged validator is complete.

- [ ] **Step 3: Implement byte-level interval validation before lifetime start**

Use an external interval list:

```cpp
struct OwningInterval {
    std::uint32_t begin;
    std::uint32_t end;
    std::size_t alignment;
    std::string_view label;
};
```

Every candidate range must pass checked `count * sizeof(T)`, checked `offset + extent`, payload bounds, alignment, null-if-and-only-if-zero, and pairwise non-overlap. A copied-byte buffer begins in `copied_bytes_unvalidated`; no candidate in that path is passed to `std::start_lifetime_as` or `std::start_lifetime_as_array` before these byte checks. A builder result begins in `constructed_unvalidated`; its lifetimes already exist from final-location construction, so validation checks the same intervals and semantics but never starts them a second time.

- [ ] **Step 4: Implement the exact staged order**

Implement the private `WorldRegionValidator` in this order. Each "start" is conditional on `copied_bytes_unvalidated`; the constructed path advances through the identical validation stages using its already-live objects:

```text
1. validate and reserve root interval; start WorldSnapshot lifetime
2. read live root descriptors; validate entity/index/party ranges pairwise and against root; reserve all three
3. start Entity[] lifetime
4. read each live Entity.name descriptor; validate against every reserved range and prior name; reserve and start each char[]
5. start EntityIndexEntry[] and EntityRelativePtr[] lifetimes
6. validate strict sorted unique index and full one-to-one ID mapping
7. validate every owner, target, party, and local_player raw offset against exact Entity element starts
8. mark the buffer validated and expose RegionView
```

The validator computes offsets from the byte-storage base and does not reconstruct a candidate through `std::uintptr_t` arithmetic.

- [ ] **Step 5: Make `load_checkpoint()` establish safe typed access**

After envelope validation, allocate a fresh `RegionBuffer`, `std::memcpy` exactly the verified payload into its storage, set used/root metadata and `copied_bytes_unvalidated`, and call `validate_and_freeze_world()`. Builder `finish()` instead sets `constructed_unvalidated`. Do not call `RegionBuffer::view()`, `world_root()`, `RegionView::elements`, or `RegionView::resolve` until the validator has completed and set `validated`.

Implement `build_canonical_world()` by creating a builder, calling Task 3's `populate_canonical_world()`, finishing with the returned root handle, and passing the resulting buffer through `validate_and_freeze_world()` before return. Implement `save_checkpoint()` as a thin precondition-checked wrapper around Task 4's `encode_checkpoint()`; reject an unvalidated `RegionBuffer`.

Define `WorldRegionAccess` in `world_runtime.cpp` and use its friendship solely to implement `world_root(const RegionBuffer&)` for the validated `WorldSnapshot` schema. Do not expose its unchecked typed access or an arbitrary root template in a header.

- [ ] **Step 6: Run all focused validation tests**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_relocatable_region test_relocatable_checkpoint test_relocatable_world -j2 && ctest --test-dir build-relocatable-world -R "^test_relocatable_(region|checkpoint|world)$" --output-on-failure'
```

Expected: PASS. Confirm every malformed fixture reports its intended `rejection_layer` rather than merely throwing.

- [ ] **Step 7: Commit validated loading**

```bash
git add -- example/relocatable_world_demo/region_storage.hpp example/relocatable_world_demo/world_runtime.hpp example/relocatable_world_demo/world_runtime.cpp example/relocatable_world_demo/checkpoint.hpp example/relocatable_world_demo/checkpoint.cpp test/test_relocatable_checkpoint.cpp test/test_relocatable_world.cpp
git commit -m "feat: add validated relocatable world loading"
```

### Task 6: Add Strict Local Agreement and Generated Fixtures

**Files:**
- Create: `example/relocatable_world_demo/agreement.hpp`
- Create: `example/relocatable_world_demo/export_signatures.cpp`
- Create: `example/relocatable_world_demo/sigs/producer_ok.sig.hpp`
- Create: `example/relocatable_world_demo/sigs/producer_packed.sig.hpp`
- Modify: `test/test_relocatable_world.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: four-key world contract and existing `SigExporter`/`PlatformInfo`.
- Produces:
  - `enum class agreement_result { match, differ, incomplete }`
  - `agreement_result check_current_agreement(PlatformInfo)`
  - `std::array<named_agreement, 4> current_agreement_details(PlatformInfo)`
  - normal and packed fixture headers

- [ ] **Step 1: Add fixture includes and strict result-shape assertions**

Extend `test/test_relocatable_world.cpp`:

```cpp
#include "agreement.hpp"
#include "sigs/producer_ok.sig.hpp"
#include "sigs/producer_packed.sig.hpp"

void test_local_agreement() {
    const auto normal =
        boost::typelayout::platform::producer_ok::get_platform_info();
    const auto packed =
        boost::typelayout::platform::producer_packed::get_platform_info();

    assert(check_current_agreement(normal) == agreement_result::match);
    assert(check_current_agreement(packed) == agreement_result::differ);

    const auto details = current_agreement_details(packed);
    assert(details[0].key == "WorldSnapshot" && details[0].matches);
    assert(details[1].key == "Entity" && !details[1].matches);
    assert(details[2].key == "EntityRelativePtr" && details[2].matches);
    assert(details[3].key == "EntityIndexEntry" && details[3].matches);
}
```

Also construct synthetic `PlatformInfo` values with a missing key, duplicate key, extra key, and `type_count != 4`; every malformed registry must return `incomplete`.

- [ ] **Step 2: Run and verify missing fixture/header failure**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_relocatable_world -j2'
```

Expected: FAIL because `agreement.hpp` and the new fixture headers do not exist.

- [ ] **Step 3: Implement strict key-based Agreement**

Require exactly four unique entries and reject extra or duplicate keys as `incomplete`. For each stable key, compare the current `get_layout_signature<T>()` with the matching fixture entry and require its `byte_copy_safe` flag. Never compare registry positions.

- [ ] **Step 4: Add normal and packed exporter targets**

`export_signatures.cpp` accepts `OUTPUT_DIRECTORY [PLATFORM_ID]`. When `PLATFORM_ID` is omitted, use `producer_ok` or `producer_packed` according to `TYPELAYOUT_RELOCATABLE_WORLD_PACKED_ENTITY`. Before every `add_relocatable<T>()`, compile-time assert whole-region Admission:

```cpp
for_each_contract_type([&]<typename T>(std::string_view key) {
    static_assert(boost::typelayout::is_admitted_v<
        T, boost::typelayout::TransferProfile::whole_region_relocation>);
    exporter.add_relocatable<T>(std::string(key));
});
```

Add unguarded `EXCLUDE_FROM_ALL` targets `relocatable_world_export_ok` and `relocatable_world_export_packed`; both compile the exporter directly against `typelayout` and the demo headers rather than linking the normally compiled runtime support library. Define the packed macro only on the packed target so no translation unit with normal `Entity` is linked into that executable. Do not add a vendor include directory.

- [ ] **Step 5: Generate rather than hand-edit both fixtures**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target relocatable_world_export_ok relocatable_world_export_packed -j2 && ./build-relocatable-world/relocatable_world_export_ok example/relocatable_world_demo/sigs && ./build-relocatable-world/relocatable_world_export_packed example/relocatable_world_demo/sigs'
```

Expected: two headers with exactly four unique keys. The packed header differs from normal only for the `Entity` signature; packed Admission still compiled successfully.

- [ ] **Step 6: Run Agreement tests**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_relocatable_world -j2 && ctest --test-dir build-relocatable-world -R "^test_relocatable_world$" --output-on-failure'
```

Expected: PASS.

- [ ] **Step 7: Commit Agreement evidence**

```bash
git add -- CMakeLists.txt example/relocatable_world_demo/agreement.hpp example/relocatable_world_demo/export_signatures.cpp example/relocatable_world_demo/sigs/producer_ok.sig.hpp example/relocatable_world_demo/sigs/producer_packed.sig.hpp test/test_relocatable_world.cpp
git commit -m "feat: gate relocatable world on layout agreement"
```

### Task 7: Complete the Positive Flow and Exactly Three Visible Negatives

**Files:**
- Create: `example/relocatable_world_demo/demo.cpp`
- Modify: `example/relocatable_world_demo/world_runtime.hpp`
- Modify: `example/relocatable_world_demo/world_runtime.cpp`
- Modify: `test/test_relocatable_world.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: validated builder/loader, Agreement fixtures, and `RegionView` access.
- Produces:
  - `capture_world_offsets(const RegionBuffer&) -> std::array<std::uint32_t, 7>`
  - `party_total_hp(const RegionBuffer&) -> std::int32_t`
  - `find_entity(const RegionBuffer&, std::uint64_t) -> const Entity&`
  - `set_world_tick(RegionBuffer&, std::uint64_t)`
  - `set_entity_hp(RegionBuffer&, std::uint64_t, std::int32_t)`
  - `canonical_graph_matches(const RegionBuffer&) -> bool`
  - complete `relocatable_world_demo` executable

- [ ] **Step 1: Add positive relocation and mutation assertions**

Extend `test/test_relocatable_world.cpp` with A-to-B-to-C behavior:

```cpp
auto source_a = build_canonical_world();
const auto offsets_a = capture_world_offsets(source_a);
const auto* base_a = source_a.used_bytes().data();
const auto checkpoint_a = save_checkpoint(source_a);

auto loaded_b = load_checkpoint(checkpoint_a);
assert(loaded_b.used_bytes().data() != base_a);
assert(capture_world_offsets(loaded_b) == offsets_a);
assert(party_total_hp(loaded_b) == 420);
assert(canonical_graph_matches(loaded_b));

set_world_tick(loaded_b, 43);
set_entity_hp(loaded_b, boss_id, 250);
auto loaded_c = load_checkpoint(save_checkpoint(loaded_b));
assert(world_root(loaded_c).tick == 43);
assert(find_entity(loaded_c, boss_id).hp == 250);
```

Keep source A alive until B's different base has been asserted.

- [ ] **Step 2: Add exact layer assertions for the three public negatives**

Define `NativePointerEntity { std::uint64_t id; Entity* target; }` and compile-time assert its whole-region Admission failure. For the packed fixture require only `Entity` to differ. For the corrupt-offset case, decode the root offset from envelope byte 20, locate the first byte of `local_player` at `checkpoint_header_size + root_offset + offsetof(WorldSnapshot, local_player)`, write `0xffffffffu` there in little-endian order, then assert `load_checkpoint()` throws `checkpoint_error` at `rejection_layer::graph`.

- [ ] **Step 3: Run tests and verify they fail before helpers exist**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target test_relocatable_world -j2'
```

Expected: FAIL because the business and offset-capture helpers are undefined.

- [ ] **Step 4: Implement only the approved business operations**

Use `world_root`, `RegionView::elements`, `RegionView::map`, `RegionView::text`, and `RegionView::resolve` for reads. Extend only the file-local `WorldRegionAccess` introduced in Task 5; it checks `validated` and implements the two public setters by touching only `WorldSnapshot::tick` or the located `Entity::hp`. Expose no mutable root, entity, span, pointer, descriptor, index, ID, or link. Implement no inventory, growth, insert/erase, general query engine, or schema migration. `capture_world_offsets()` returns, in order, Hero owner/target, Boss owner/target, party[0], party[1], and local player raw plus-one values.

- [ ] **Step 5: Implement the linear talk executable**

At the top of `demo.cpp`, retain this source attribution without adding runtime output:

```cpp
// A self-contained teaching example inspired by offset-based arena and
// checkpoint designs, including XOffsetDatastructure. It is not
// XOffsetDatastructure and does not implement or validate its wire format.
```

`demo.cpp` performs Admission and normal Agreement before any checkpoint load, runs the positive A-to-B-to-C flow, then executes the three negatives. Its successful stdout is exactly equivalent to:

```text
Admission[whole_region_relocation]: PASS
Agreement[producer_ok, 4 types]: MATCH
Relocation: base changed, raw offsets unchanged
Graph: null + shared + cycle + pointer container PASS
Business: party_hp=420, tick=42->43, boss_hp=300->250
Reload: mutation persisted

Negative[native pointer]: Admission FAIL, load skipped
Negative[packed Entity]: Agreement DIFFER, load skipped
Negative[corrupt region offset]: graph REJECT before dereference
```

Return non-zero on any missing fixture, unexpected Agreement shape, validation failure, incorrect relationship, or wrong business value.

- [ ] **Step 6: Register and run the demo**

Add an unguarded `relocatable_world_demo` target linked to `relocatable_world_support`, register CTest label `typelayout;example;relocatable-world`, then run:

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target relocatable_world_demo test_relocatable_world -j2 && ./build-relocatable-world/relocatable_world_demo | tee build-relocatable-world/relocatable_world_demo.out && ctest --test-dir build-relocatable-world -R "^(relocatable_world_demo|test_relocatable_world)$" --output-on-failure'
```

Use fixed-string checks for all nine non-empty output lines and verify no fourth `Negative[` line exists.

- [ ] **Step 7: Commit the complete local demo**

```bash
git add -- CMakeLists.txt example/relocatable_world_demo/demo.cpp example/relocatable_world_demo/world_runtime.hpp example/relocatable_world_demo/world_runtime.cpp test/test_relocatable_world.cpp
git commit -m "feat: demonstrate relocatable world checkpoint"
```

### Task 8: Retire the XOffset-Backed Demo and Run Full Regression

**Files:**
- Delete: `example/xoffset_world_demo/demo.cpp`
- Delete: `example/xoffset_world_demo/export_signatures.cpp`
- Delete: `example/xoffset_world_demo/world.hpp`
- Delete: `example/xoffset_world_demo/sigs/producer_ok.sig.hpp`
- Delete: `example/xoffset_world_demo/sigs/producer_packed.sig.hpp`
- Modify: `CMakeLists.txt:73-107`
- Preserve unchanged: `.gitmodules`
- Preserve unchanged: `vendor/XOffsetDatastructure`

**Interfaces:**
- Consumes: all passing new targets from Tasks 1-7.
- Produces: one default-build standalone demo with no XOffset build dependency or silent skip.

- [ ] **Step 1: Prove the new replacement is green before deletion**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world --target relocatable_world_demo test_relocatable_region test_relocatable_checkpoint test_relocatable_world -j2 && ctest --test-dir build-relocatable-world -R "^(relocatable_world_demo|test_relocatable_region|test_relocatable_checkpoint|test_relocatable_world)$" --output-on-failure'
```

Expected: PASS before old files are removed.

- [ ] **Step 2: Remove only the old example and guarded CMake block**

Delete the old directory files and remove `TYPELAYOUT_XOFFSET_HEADER`, the Clang-only conditional, vendor `EXISTS` conditional, skip message, the three old targets, old packed macro, and `typelayout;example;xoffset` label. Do not remove or modify `.gitmodules` or the vendor gitlink.

- [ ] **Step 3: Reconfigure from a clean build directory and test without the submodule**

Use a fresh build directory and do not initialize submodules:

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && cmake -S . -B build-relocatable-world-final -G Ninja -DCMAKE_CXX_COMPILER=/root/clang-p2996-install/bin/clang++ -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++" -DTYPELAYOUT_BUILD_COMPAT_CI=OFF && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world-final -j2 && ctest --test-dir build-relocatable-world-final --output-on-failure'
```

Expected: all existing and new default tests PASS; configuration contains no XOffset skip message.

- [ ] **Step 4: Verify generated fixtures are reproducible**

```bash
wsl -e bash -lc 'cd /mnt/e/workspace/TypeLayout/.worktrees/cppcon2026-deck && export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && cmake --build build-relocatable-world-final --target relocatable_world_export_ok relocatable_world_export_packed -j2 && cmake -E make_directory build-relocatable-world-final/fixture-check && ./build-relocatable-world-final/relocatable_world_export_ok build-relocatable-world-final/fixture-check && ./build-relocatable-world-final/relocatable_world_export_packed build-relocatable-world-final/fixture-check && diff -I "^// Generated:" example/relocatable_world_demo/sigs/producer_ok.sig.hpp build-relocatable-world-final/fixture-check/producer_ok.sig.hpp && diff -I "^// Generated:" example/relocatable_world_demo/sigs/producer_packed.sig.hpp build-relocatable-world-final/fixture-check/producer_packed.sig.hpp'
```

Expected: no differences other than the generated timestamp.

- [ ] **Step 5: Run scope and cleanliness checks**

```powershell
$forbidden = rg -n "xoffset_world|TYPELAYOUT_XOFFSET|xoffsetdatastructure.hpp|XBuffer|XString|XVector|XMap" CMakeLists.txt example test
if ($LASTEXITCODE -eq 0) { throw "XOffset dependency remains`n$forbidden" }
if ($LASTEXITCODE -ne 1) { throw "XOffset search failed" }
$opaque = rg -n "TYPELAYOUT_(REGISTER_OPAQUE|OPAQUE_TYPE)" example/relocatable_world_demo
if ($LASTEXITCODE -eq 0) { throw "opaque registration remains`n$opaque" }
if ($LASTEXITCODE -ne 1) { throw "opaque search failed" }
git ls-files --stage -- .gitmodules vendor/XOffsetDatastructure
$baseline = Get-Content -Raw build-relocatable-world-baseline/commit.txt
git diff --exit-code "$baseline..HEAD" -- .gitmodules vendor/XOffsetDatastructure docs/talk/cppcon2026-sched-listing.md docs/talk/cppcon2026-main-deck-content-and-script.md docs/superpowers/specs/2026-08-23-cppcon2026-typelayout-deck-design.md docs/superpowers/plans/2026-08-23-cppcon2026-typelayout-deck-implementation.md
if ($LASTEXITCODE -ne 0) { throw "protected tracked path changed in implementation commits" }
git diff --exit-code -- .gitmodules vendor/XOffsetDatastructure docs/talk/cppcon2026-sched-listing.md docs/superpowers/specs/2026-08-23-cppcon2026-typelayout-deck-design.md docs/superpowers/plans/2026-08-23-cppcon2026-typelayout-deck-implementation.md
if ($LASTEXITCODE -ne 0) { throw "protected tracked path has working-tree changes" }
$deckHash = git hash-object docs/talk/cppcon2026-main-deck-content-and-script.md
if ($LASTEXITCODE -ne 0) { throw "pre-existing main-deck file is missing" }
$expectedDeckHash = Get-Content -Raw build-relocatable-world-baseline/main-deck.sha
if ($deckHash -ne $expectedDeckHash) { throw "pre-existing main-deck bytes changed" }
git diff --check
git status --short
```

Expected: the first two searches have no matches and the vendor gitlink remains `2233004983cd42664e3d6084ec09092b2968ad4e`. The pre-existing user-owned change to `docs/talk/cppcon2026-main-deck-content-and-script.md` may still appear in status; preserve its captured bytes and never stage it as part of this work. Review all other status entries against this plan's explicit file lists.

- [ ] **Step 6: Commit the replacement cleanup**

```bash
git add -- CMakeLists.txt example/xoffset_world_demo example/relocatable_world_demo test/test_relocatable_region.cpp test/test_relocatable_checkpoint.cpp test/test_relocatable_world.cpp
git commit -m "refactor: retire XOffset-backed world demo"
```

## Core Completion Gate

Do not begin the matrix plan until all of the following are true:

```text
test_core PASS
test_gate_negative PASS
compat_check_demo_negative PASS
the complete freshly configured CTest suite PASS
test_relocatable_region PASS
test_relocatable_checkpoint PASS
test_relocatable_world PASS
relocatable_world_demo PASS
normal fixture reproducible
packed fixture reproducible and Entity-only DIFFER
no XOffset include/link/configure dependency
vendor gitlink unchanged
deck files unchanged
tracked protected paths unchanged from the recorded baseline commit
pre-existing main-deck working bytes match the captured hash
```

At that point the local demo is independently useful and the matrix plan may consume its stable `.region`, contract registry, producer-name override, validation-layer diagnostics, and canonical-world assertions.
