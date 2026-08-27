# Relocatable World Checkpoint Demo Design

**Status:** Approved for implementation on 2026-08-27

**Date:** 2026-08-27

**Replaces:** `2026-08-27-xoffset-world-demo-design.md`

**Inspiration reference:** XOffsetDatastructure `next_cpp26` at `2233004983cd42664e3d6084ec09092b2968ad4e`

## 1. Purpose

Add one self-contained TypeLayout demo that models a game-server world checkpoint stored as a relocatable byte region. The checkpoint contains dynamic-length strings and collections plus a graph with null, shared, cyclic, and container-stored relative pointers. It can be saved, loaded at a different base address, queried, modified in place, and saved again without pointer fixups.

The demo proves two independent gates before application data is loaded:

1. **Admission:** every declared native representation is valid for the explicitly selected whole-region relocation profile.
2. **Agreement:** every declared producer and consumer build gives those representations the same TypeLayout signatures.

After both gates permit transfer, a small demo-local loader validates the checkpoint envelope and every stored range. A graph validator then checks application-defined relative pointers before any dereference.

This is a representative practical boundary, not a production format. It is deliberately small enough to explain in a talk while retaining the feature that makes offset-based regions useful: movable pointers compose into strings, collections, lookup structures, shared targets, and cycles.

This is an explicitly separate whole-region-relocation appendix demo. It cannot replace the deck's Stage 5 `portable_capture` ordinary-copy walkthrough or its required real platform-divergent negative. The packed-`Entity` fixture is supplemental ABI-setting evidence for this relocation demo, not the platform-divergent example promised by the public session.

## 2. Relationship to XOffsetDatastructure

The scenario and region-relative design are inspired by XOffsetDatastructure and other offset-based arena, shared-memory, and checkpoint systems. The demo is independently implemented:

- it does not include or link `xoffsetdatastructure.hpp`;
- it does not use `XBuffer`, `XString`, `XVector`, `XMap`, XOffset allocators, schema macros, or its wire loader;
- its checkpoint is not XOffset wire-format compatible;
- it does not demonstrate that TypeLayout is integrated into XOffsetDatastructure.

The repository may retain `vendor/XOffsetDatastructure` pinned to `next_cpp26` as a reference source, but the demo, its CMake targets, local launcher, and CI do not initialize or depend on that submodule. No XOffset GCC port is required by this design.

The source and deck notes must use wording equivalent to:

> A self-contained teaching example inspired by offset-based arena and checkpoint designs, including XOffsetDatastructure. It is not XOffsetDatastructure and does not implement or validate its wire format.

The executable and directory are named `relocatable_world_demo`. The old `xoffset_world_demo` and `.xbuf` names are not retained. No XOffset source code is copied. If implementation later needs to adapt XOffset code rather than merely its ideas, that is a design change and requires its MIT notice and explicit attribution.

## 3. Success Criteria

The local demo must show:

- a world with two entities, dynamic-length names, a dynamic entity collection, an ID index, and a collection of relative pointers;
- null, non-null, shared, cyclic, and collection-stored relative links;
- whole-region Admission for the four explicitly declared payload types;
- Agreement against a normal producer fixture;
- save and load with source and destination regions simultaneously alive at different base addresses;
- unchanged raw region-relative offsets and correct relationships after relocation without fixups;
- an ID lookup, party-health query, tick update, boss-health update, and second save/load;
- native-pointer rejection at Admission;
- packed-`Entity` rejection at Agreement;
- corrupted-region-offset rejection before dereference.

The native evidence matrix must additionally show:

- six real 64-bit little-endian producer builds;
- six consumers, each evaluating the other five producer checkpoints;
- exactly 15 unordered Agreement decisions;
- exactly 30 directed transfer decisions and, in a permitting run, 30 successful cross-loads;
- complete build and artifact provenance;
- four named TypeLayout `PERMIT` decisions per agreeing build pair, plus a separate successful workflow closure only when all declared runtime evidence is complete.

No deck file is changed until the implementation and evidence exist. Deck-facing observations remain notes until then.

## 4. Scope and Non-Goals

### 4.1 In Scope

- One four-byte `relative_ptr<T>`.
- Three frozen region containers: `region_string`, `region_vector<T>`, and `region_flat_map<K, V>`.
- A fixed-capacity, one-allocation `RegionBuilder` used only during construction.
- An aligned `RegionBuffer` that owns one contiguous payload.
- A small, explicitly byte-encoded checkpoint envelope.
- Structural validation for the envelope and every declared region range.
- Application validation for relative pointers and the ID index.
- The existing minimal TypeLayout whole-region Admission profile.
- Four explicit Agreement keys.
- The local positive path and exactly three negative demonstrations.
- Six-node native producer/consumer evidence.

### 4.2 Explicit Non-Goals

- A general allocator, free list, compactor, garbage collector, or memory-mapped database.
- Container growth, insertion, erasure, or reallocation after the region is frozen.
- Source-compatible replacements for XOffset or STL containers.
- Reflection-driven object construction.
- A generic region schema language, semantic dependency closure, or migration framework.
- Hostile-input hardening beyond the explicitly declared format and validation checks.
- Schema evolution, semantic compatibility, cross-endian conversion, or a universal wire format.
- Independent movement of a container header, its elements, or its pointees.
- Arbitrary non-trivially-copyable object relocation.
- Windows, mobile, big-endian, or 32-bit nodes.
- Claims about production NetEase formats or real XOffset deployments.

## 5. Practical Scenario

The checkpoint represents a trusted game-server world exchanged under one application and schema contract for process takeover:

- `tick == 42` initially;
- `Hero` has ID 1001 and HP 120;
- `Boss` has ID 2001 and HP 300;
- `Hero.owner` is null;
- `Boss.owner` points to `Hero`;
- `Hero.target` points to `Boss`;
- `Boss.target` points to `Hero`, completing a cycle;
- `local_player`, `Boss.owner`, and `party[0]` share `Hero`;
- `party` contains `[Hero, Boss]` and has total HP 420;
- `entity_index` maps the two stable IDs to indices 0 and 1.

After loading at a new base, the consumer changes the tick from 42 to 43 and Boss HP from 300 to 250, saves again, and proves the mutation survives another load.

This graph is the smallest one that covers every required pointer shape without adding unrelated domain concepts.

## 6. Stored Data Model

All stored fields use fixed-width representation types:

```cpp
struct Position {
    std::int32_t x;
    std::int32_t y;
};

enum class EntityKind : std::uint8_t {
    player,
    boss
};

struct Entity {
    std::uint64_t id;
    EntityKind kind;
    Position position;
    std::int32_t hp;
    region_string name;
    relative_ptr<Entity> owner;
    relative_ptr<Entity> target;
};

using EntityIndexEntry =
    region_key_value<std::uint64_t, std::uint32_t>;

struct WorldSnapshot {
    std::uint64_t tick;
    region_vector<Entity> entities;
    region_flat_map<std::uint64_t, std::uint32_t> entity_index;
    region_vector<relative_ptr<Entity>> party;
    relative_ptr<Entity> local_player;
};
```

The stored objects are standard-layout, trivially copyable, and implicit-lifetime types. Standard-library containers, spans, strings, iterators, allocators, and process addresses are never stored in the payload.

## 7. Relative Representations

### 7.1 `relative_ptr<T>`

`relative_ptr<T>` contains exactly one member:

```cpp
std::uint32_t offset_plus_one_ = 0;
```

Its contract is:

- zero encodes null;
- a non-zero value minus one is the target's byte offset from the beginning of the containing region;
- its size and alignment are exactly four bytes;
- construction-time reset is private and is invoked only by `RegionBuilder::bind(relative_ptr<T>&, region_handle<T>)`; that operation accepts a null handle or a handle issued by the same active builder to either a complete standalone `T` object or one complete live `T` array element, and it never accepts an address inside such an object;
- `raw_offset_plus_one()` is available for validation and demonstration;
- resolution always receives a validated `RegionView`; there is no context-free `get()`;
- `RegionView::resolve(pointer)` first requires the `relative_ptr` descriptor itself to belong to that same validated buffer, then performs checked byte-array offset arithmetic from the region base and returns a const typed pointer only after the target allocation has been validated.

The plus-one representation avoids collision between a valid object at payload offset zero and the null value. The fixed 4096-byte region is far below the largest representable non-null offset.

Its TypeLayout signature reflects the real `std::uint32_t` member. It uses no opaque token and does not recursively expand `T`. Runtime object-graph recursion and representation-signature recursion remain separate.

### 7.2 `region_vector<T>`

`region_vector<T>` is a frozen descriptor:

```cpp
template <typename T>
struct region_vector {
    relative_ptr<T> data_{};
    std::uint32_t size_ = 0;
};
```

Its stored representation is exactly one relative pointer and one element count. It has no stored allocator and no capacity. An empty vector has a null pointer and zero size; a non-empty vector has a non-null, correctly aligned range containing exactly `size_` live `T` elements.

After validation it exposes only `size()`, `operator[]`, `begin()`, and `end()`. Construction is performed by `RegionBuilder`. It has no `reserve`, `emplace_back`, insertion, erasure, or resize operation.

### 7.3 `region_string`

`region_string` has the same two-field representation as a character vector:

```cpp
struct region_string {
    relative_ptr<char> data_{};
    std::uint32_t size_ = 0;
};
```

It stores exactly `size_` bytes and does not require a trailing null. After validation it exposes a `std::string_view`. The demo values are ASCII, while string encoding is outside TypeLayout's representation claim.

### 7.4 `region_flat_map<K, V>`

`region_flat_map<K, V>` is a frozen sorted collection:

```cpp
template <typename K, typename V>
struct region_key_value {
    K key;
    V value;
};

template <typename K, typename V>
struct region_flat_map {
    region_vector<region_key_value<K, V>> entries_;
};
```

The validator requires keys to be strictly increasing and therefore unique. After validation `find(key)` performs binary search. There is no mutation of map topology after construction.

These are practical region containers, not reduced spellings of inline fixed arrays: their element and character counts are encoded at runtime and their payloads live elsewhere in the same region.

All four descriptor families keep trivial copy construction so their representations remain trivially copyable, but their copy and move assignment operators are inaccessible to callers. Trusted view/validator friends never assign them; only `RegionBuilder` writes topology. The builder never exposes a mutable construction reference, so callers cannot overwrite a bound descriptor, an `Entity`, or a `WorldSnapshot` with descriptor bytes copied from another builder. Ordinary scalar and map-entry initialization remains available only through the checked Admission-constrained `set` operations where it carries no region link.

## 8. Region Construction and Lifetime

### 8.1 `RegionBuffer`

`RegionBuffer` owns an `alignas(64)` storage object containing a 4096-element `std::byte` array. That array provides storage for all nested region objects and gives validation and resolution one well-defined base for byte-offset arithmetic. Every stored type must have alignment no greater than 64. The buffer exposes a byte span for validation and serialization; only the schema-bound `world_root(const RegionBuffer&)` adapter exposes the typed root after successful validation. There is no public arbitrary `root<T>()` cast.

The payload is zero-initialized before objects are constructed so untouched storage does not expose previous memory contents. Agreement covers the location and extent of any padding, but the demo does not interpret padding bytes or require their values to be canonical. The builder establishes every single object directly in its final aligned location and populates its members in place.

Loading allocates a fresh aligned storage object and copies the envelope-checked payload with `std::memcpy`, never `std::copy` or a hand-written byte loop. Typed access does not depend on which implicit objects that copy might create. The loader first checks root bounds and alignment through the byte view, explicitly establishes the `WorldSnapshot` lifetime with `std::start_lifetime_as<WorldSnapshot>`, and then establishes each dynamic array lifetime with `std::start_lifetime_as_array<T>` in the staged order below. No array indexing, iteration, or member access occurs before the corresponding lifetime exists. Support for both lifetime-start operations is part of the toolchain probe. No destructor traversal is required.

### 8.2 `RegionBuilder`

`RegionBuilder` is not stored in the payload. It returns construction-only `region_handle<T>` values containing checked payload offsets and provides only:

- aligned allocation and lifetime start for one object or one actual contiguous array;
- Admission-constrained ordinary field and array-element writes addressed by construction handle plus pointer-to-member, returning no reference or pointer;
- handle/member initialization of a `region_string`, `region_vector<T>`, or `region_flat_map<K, V>` and its final storage;
- handle/member binding of a `relative_ptr<T>` to a same-builder construction handle, including party array elements;
- schema-bound finalization from `region_handle<WorldSnapshot>` only, recording the root offset and used byte count.

No mutable typed reference, pointer, or span leaves the builder. Native pointers and region descriptors are not eligible for the ordinary-write path; descriptors and links use only their dedicated checked operations. Every member operation rejects a null pointer-to-member before dereference, and every operation rechecks that the builder is active and that all destination and non-null source handles belong to it. Retaining construction handles is harmless because finalization permanently closes the builder before validation.

The demo uses a 4096-byte initial capacity. Exceeding it is an error; the underlying region never reallocates.

`align_up`, `count * sizeof(T)`, cursor advancement, and capacity conversion are overflow-checked before storage is reserved. Construction uses `std::start_lifetime_as<T>` for single objects and `std::start_lifetime_as_array<T>` for arrays, so every dynamic collection is an actual C++ array object. Stored descriptors and already-linked entities or party entries are populated only in their final locations; they are not copied from linked temporaries.

Construction order is strict:

1. allocate the region and root;
2. allocate and populate entities, names, index entries, and party entries through handle/member operations;
3. obtain checked element handles after all final-location allocation;
4. bind every relative pointer through those handles;
5. validate the complete region;
6. freeze topology.

After freezing, public reads are const and require descriptors from the same validated buffer. The only public writes are the schema-bound `set_world_tick(...)` and `set_entity_hp(...)` operations; generic root, descriptor, link, index, and topology mutation is unavailable.

## 9. Checkpoint Envelope

The envelope is encoded and decoded field-by-field as canonical bytes rather than copied from a native C++ header struct. It therefore does not need a TypeLayout Agreement key.

Version 1 is exactly 40 bytes:

| Byte offset | Width | Encoding | Value |
|---:|---:|---|---|
| 0 | 8 | bytes | `TLWORLD\0` |
| 8 | 2 | unsigned little-endian | version `1` |
| 10 | 2 | unsigned little-endian | header size `40` |
| 12 | 4 | bytes | format tag `64LE` |
| 16 | 4 | unsigned little-endian | used payload bytes |
| 20 | 4 | unsigned little-endian | root offset |
| 24 | 4 | unsigned little-endian | flags, exactly zero |
| 28 | 4 | unsigned little-endian | reserved, exactly zero |
| 32 | 8 | bytes | schema tag `WORLDV1\0` |

The decoder uses checked conversion from every encoded integer to `std::size_t` and requires the artifact size to equal exactly `40 + used_payload_bytes`. Truncated input, trailing input, a payload larger than 4096 bytes, or a root outside the used payload is rejected.

There is no allocator state and no payload checksum. Artifact hashes belong in CI provenance, not the runtime format. Omitting a checksum deliberately allows the corrupted-region-offset negative to pass envelope validation and be rejected by the graph validator at the intended layer.

`save_checkpoint()` emits envelope plus used payload bytes. `load_checkpoint()` validates the envelope, allocates a different aligned region, copies the payload, and validates every descriptor before returning typed access.

## 10. TypeLayout Admission and Agreement

### 10.1 Admission

The demo retains the public TypeLayout distinction between:

- `ordinary_copy`: source-address-independent representations only;
- `whole_region_relocation`: same-region relative representations are permitted when the complete region is copied to a new base while preserving its single region-relative offset space.

The demo-local source-context traits are:

- `relative_ptr<T>`: `same_region`; do not expand `T`;
- `region_string`: `same_region`;
- `region_vector<T>`: `join(same_region, source_context_v<T>)`;
- `region_key_value<K, V>`: inferred from `K` and `V`;
- `region_flat_map<K, V>`: inferred through its entry vector;
- `Entity` and `WorldSnapshot`: inferred through their actual members.

All stored descriptors are trivially copyable, so no non-trivial lifetime opt-in is needed. Their relocation traits still close the semantic dependency on dynamically stored elements:

```text
region_relocation_traits<region_string>::enabled =
    is_admitted_v<char, whole_region_relocation>

region_relocation_traits<region_vector<T>>::enabled =
    is_admitted_v<T, whole_region_relocation>

region_relocation_traits<region_key_value<K, V>>::enabled =
    is_admitted_v<K, whole_region_relocation>
    && is_admitted_v<V, whole_region_relocation>

region_relocation_traits<region_flat_map<K, V>>::enabled =
    is_admitted_v<region_key_value<K, V>, whole_region_relocation>

region_relocation_traits<Entity>::enabled =
    admitted<uint64_t>
    && admitted<EntityKind>
    && admitted<Position>
    && admitted<int32_t>
    && admitted<region_string>
    && admitted<relative_ptr<Entity>>

region_relocation_traits<WorldSnapshot>::enabled =
    admitted<uint64_t>
    && admitted<region_vector<Entity>>
    && admitted<region_flat_map<uint64_t, uint32_t>>
    && admitted<region_vector<relative_ptr<Entity>>>
    && admitted<relative_ptr<Entity>>
```

Here `admitted<T>` abbreviates `is_admitted_v<T, whole_region_relocation>`. Consequently, a small container header cannot admit an undeclared unsafe element representation, and the root closes through every concrete dynamic element type. A native pointer remains `address_space_dependent` and fails whole-region Admission.

The recursion is finite: `WorldSnapshot` reaches `Entity` through `region_vector<Entity>`, while `Entity` reaches only the concrete integer representation of `relative_ptr<Entity>` and never recursively expands its pointee.

### 10.2 Finite Agreement Contract

The explicit payload contract has four keys:

| Evidence key | Stored representation |
|---|---|
| `WorldSnapshot` | root plus all embedded container descriptors |
| `Entity` | dynamic entity element, position, kind, name descriptor, and links |
| `EntityRelativePtr` | dynamic `party` element |
| `EntityIndexEntry` | dynamic flat-map element |

`Position`, `EntityKind`, and `region_string` need no separate keys because they are embedded in `Entity`. The other container descriptors are embedded in `WorldSnapshot`. Character payloads are raw bytes. The explicit element keys close the representations that live outside the root object.

Signature comparison joins by these stable names, not registry position. A missing entry is `INCOMPLETE`; a present unequal entry is `REJECT` with diagnostic `Agreement DIFFER; load skipped`.

The checked-in normal and packed producer fixtures remain the compact local evidence. The packed fixture changes only `Entity` and is not a platform node.

## 11. Validation and Trust Boundary

The demo accepts trusted checkpoints under one application and schema contract, not arbitrary hostile input. Validation has three layers:

1. **Admission and Agreement** run before calling the loader.
2. **Envelope and region validation** check the byte format and every declared range.
3. **World graph validation** checks relative entity links and the ID index before application dereference.

Envelope and region validation require:

- correct magic, version, format tag, schema identifier, and zero reserved flags;
- artifact length exactly equal to encoded header length plus used payload bytes;
- payload and root offsets within bounds without integer overflow;
- root alignment and complete root extent;
- every string, vector, and map range within the payload;
- range multiplication and address addition without overflow;
- correct element alignment;
- null pointer if and only if the corresponding count is zero;
- the root, entity array, index-entry array, party array, and each name range to be pairwise disjoint;
- strictly increasing, unique index keys;
- index size equal to entity count;
- unique entity IDs;
- every index value smaller than the entity count;
- every entry key equal to `entities[entry.value].id`;
- every entity represented by exactly one index entry.

The owning-range disjointness rule is intentionally strict: the demo does not support string interning or overlapping collection storage. Application relative pointers are non-owning and may target exact elements inside the entity array.

Validation is staged. Before any lifetime-start operation, byte-level checks prove the candidate range's count, extent, alignment, and non-overlap with every owning range already known or reserved by an earlier descriptor. A rejected range never has its typed lifetime started. The order is:

1. decode the envelope; validate the root bounds and alignment; reserve its owning interval; then establish the root;
2. read only the now-live root descriptors; validate the entity, index-entry, and party ranges against one another and the root; reserve all three intervals; then establish the entity array;
3. read each now-live entity's name descriptor; validate each name range against every reserved interval and all earlier name ranges; reserve it; then establish that name byte array;
4. establish the already-validated index-entry and party-pointer arrays;
5. check index semantics;
6. validate every application graph link.

This two-stage handling of names is required because their descriptors cannot be read until the entity array exists, while their target ranges must be proved disjoint before the character-array lifetimes begin.

Public container iteration and `RegionView::resolve` remain unavailable until the complete sequence succeeds. Each public view operation additionally rejects a descriptor object that is not physically inside its bound buffer, preventing a descriptor from another validated region from being resolved against the wrong base. The schema validator uses private checked offset access while establishing the stages.

Graph validation reads raw offset-plus-one values first and performs checked byte-offset arithmetic within the `RegionBuffer` storage extent. It does not round-trip pointers through integers. A non-null entity link must:

1. decode to an offset without underflow or overflow;
2. lie inside the region;
3. satisfy `alignof(Entity)`;
4. equal the first byte of one live `entities[i]` object.

A pointer merely into the middle of an entity is rejected. Only after all descriptors, index entries, and graph links pass may application code call `RegionView::resolve(...)` or iterate a region container.

## 12. Positive Execution Flow

```text
build source region A
-> allocate all region storage
-> populate Hero, Boss, strings, index, and party
-> link relative pointers
-> validate A
-> require four-type Admission PASS
-> require producer_ok Agreement MATCH
-> save checkpoint A
-> keep A alive
-> load bytes into region B
-> require base(A) != base(B)
-> validate B before dereference
-> require sampled raw region offsets unchanged
-> verify null/shared/cycle/container relationships
-> query party HP == 420
-> mutate tick 42->43 and Boss HP 300->250
-> save B
-> load into region C
-> validate C
-> verify the mutations persisted
```

The source region remains live while the loaded region is checked, making the base-address difference unambiguous. No fixup pass is performed.

## 13. Required Negative Cases and Output

There are exactly three negative demonstrations:

1. A native-pointer alternative fails whole-region Admission, and loading is skipped.
2. A packing-controlled `Entity` remains locally byte-transport eligible but has a different signature, so Agreement rejects and loading is skipped.
3. One valid checkpoint has `local_player` changed to an out-of-region 32-bit region offset. Envelope and descriptor validation accept it, but graph validation rejects before dereference.

The talk-sized output remains equivalent to:

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

The packed fixture must still pass whole-region Admission. Its `WorldSnapshot`, `EntityRelativePtr`, and `EntityIndexEntry` entries must match the normal fixture; only `Entity` may differ. This pins the negative to one intentional representation change.

## 14. Six-Node Native Evidence

The authoritative matrix is:

| Node ID | Native host | GitHub runner | Toolchain |
|---|---|---|---|
| `x86_64_linux_gcc` | Linux x86-64 | `ubuntu-24.04` | GCC 16.2 plus matching libstdc++ |
| `x86_64_linux_clang` | Linux x86-64 | `ubuntu-24.04` | pinned Bloomberg Clang P2996 plus matching libc++ |
| `arm64_linux_gcc` | Linux ARM64 | `ubuntu-24.04-arm` | GCC 16.2 plus matching libstdc++ |
| `arm64_linux_clang` | Linux ARM64 | `ubuntu-24.04-arm` | pinned Bloomberg Clang P2996 plus matching libc++ |
| `arm64_macos_clang` | macOS ARM64 | `macos-15` | pinned Bloomberg Clang P2996 plus matching libc++ |
| `x86_64_macos_clang` | macOS x86-64 | `macos-15-intel` | pinned Bloomberg Clang P2996 plus matching libc++ |

Every node first compiles and runs the same reflection/platform probe. It requires `CHAR_BIT == 8`, `sizeof(void*) == 8`, little-endian native byte order, a working P2996 reflection expression, and usable `std::start_lifetime_as` plus `std::start_lifetime_as_array`. The node then emits `<node>.provenance.json` with the probe results, four Admission decisions, and verified toolchain, target, standard-library, runner, source, flags, and build identities. Every admitted producer additionally emits:

```text
<node>.sig.hpp
<node>.region
```

The provenance binds these two files by SHA256. Each independently compiled consumer emits `<consumer>.results.json` with its own verified compiler/runner/SDK facts, the hashes of evidence it evaluated, and exactly five directed-edge decisions. One closure may use only records from one source commit, one workflow invocation, and one committed source/output lock pair; mixing individually valid artifacts from different runs or borrowing a producer's environment identity for a separate consumer job is incomplete evidence.

Every unordered build pair gets one of exactly 15 Agreement records. Each record contains four named TypeLayout decisions, one for each contract key:

- `PERMIT` when that type passes Admission on both builds and its signatures match;
- `REJECT` when Admission fails or two present signatures differ;
- `INCOMPLETE` when the required Admission or signature evidence is missing or unevaluable.

Thus a successful matrix contains 60 named TypeLayout `PERMIT` decisions. `PERMIT` is reserved for this Admission-plus-Agreement decision and never includes loader or application validation.

Every ordered pair of different nodes gets one of exactly 30 separate transfer records:

- `PASS` after four TypeLayout permits, envelope validation, region validation, graph validation, and canonical-world checks;
- `SKIPPED_TYPELAYOUT_REJECT` when a complete TypeLayout decision rejects before loading;
- `REJECT_ENVELOPE`, `REJECT_REGION`, or `REJECT_GRAPH` when a later validation layer rejects;
- `INCOMPLETE` when required evidence or execution is missing.

The overall workflow closure is distinct from TypeLayout Permit:

1. `INCOMPLETE` when any required decision or evidence cannot be evaluated;
2. otherwise `REJECT` when any named TypeLayout decision or directed transfer rejects;
3. otherwise `PASS` when all six nodes, 60 named TypeLayout permits, and 30 directed loads succeed.

Consumer and Agreement jobs depend on producer completion but use `if: always()` and emit explicit missing-input results. The final closure job also uses `if: always()`, consumes the uploaded Agreement artifact as a required input, and verifies it against the same fixed producer evidence before closing the graph. A failed, skipped, stale, or malformed Agreement job cannot be bypassed by silently recomputing a smaller or different matrix inside closure.

## 15. Toolchains and Developer Experience

Linux uses two pinned multi-platform images, each supporting `linux/amd64` and `linux/arm64`. macOS uses pinned native Bloomberg P2996 archives for ARM64 and x86-64. Because those archives do not redistribute an Apple SDK, authoritative macOS jobs must select the output-lock Xcode/SDK/deployment identity and pass its sysroot explicitly; a personal Mac records its actual SDK and remains non-authoritative when it differs. `toolchain-sources.lock` contains exact source revisions, checksums, and build-recipe hashes; its changes trigger native toolchain builds. The build publishes immutable candidates and emits a candidate `toolchains.lock`. That output lock is separately reviewed and committed with image digests, release URLs, and archive checksums; sealing it does not trigger another candidate build.

The matrix and local launcher refuse to run until the sealed output lock is complete. Empty values, branch-only Bloomberg references, mutable tags such as `latest`, and unverified archives are errors.

The ARM64 Mac entry point is:

```bash
./tools/run-relocatable-world.sh
```

With no arguments it derives the exact current `HEAD` and a unique local invocation ID before doing any work. Advanced/repeatable invocation may pass `--source-sha SHA --run-id ID`; an explicitly supplied source SHA must still equal the current `HEAD`.

It runs:

- macOS ARM64 / Bloomberg Clang natively;
- Linux ARM64 / GCC and Bloomberg Clang in native-architecture Docker containers;
- Linux x86-64 / GCC and Bloomberg Clang through Docker emulation.

Its final line is equivalent to:

```text
LOCAL COVERAGE 5/6: 3 native-architecture + 2 Docker-emulated;
Agreement 10/10; directed loads 20/20; authoritative closure unavailable
```

The personal Mac is not a required self-hosted runner. GitHub-hosted native nodes provide authoritative 6/6 evidence.

The demo's independence from XOffset removes the XOffset GCC port, XOffset-specific standard-library assumptions, submodule initialization, and silent demo skipping from this workflow. It does not remove the need for pinned P2996 compilers.

## 16. Repository Change Surface

The implementation is expected to replace the current XOffset-backed example with:

```text
example/relocatable_world_demo/region.hpp
example/relocatable_world_demo/world.hpp
example/relocatable_world_demo/demo.cpp
example/relocatable_world_demo/export_signatures.cpp
example/relocatable_world_demo/producer.cpp
example/relocatable_world_demo/consumer.cpp
example/relocatable_world_demo/matrix_check.cpp
example/relocatable_world_demo/sigs/producer_ok.sig.hpp
example/relocatable_world_demo/sigs/producer_packed.sig.hpp
tools/run-relocatable-world.sh
.github/docker/Dockerfile.gcc16
.github/docker/Dockerfile.p2996
.github/docker/docker-bake.hcl
.github/docker/toolchain-sources.lock
.github/docker/toolchains.lock
.github/scripts/build-p2996-macos.sh
.github/scripts/verify-p2996-toolchain.sh
.github/workflows/toolchain-images.yml
.github/workflows/relocatable-world-matrix.yml
CMakeLists.txt
```

Once the replacement targets and tests pass, implementation removes `example/xoffset_world_demo/**` and the `xoffset_world_demo`, `xoffset_world_export_ok`, and `xoffset_world_export_packed` CMake targets. CMake must no longer test for the vendor header or silently skip this demo. The pinned vendor submodule itself remains unchanged as a non-build reference unless the user separately decides to remove it.

`include/boost/typelayout/admission.hpp` and its core tests remain because the whole-region profile is already part of the implemented base demo. They change only if the independent containers expose a real missing TypeLayout behavior.

The existing `docs/superpowers/plans/2026-08-27-xoffset-world-demo-implementation.md` records the superseded XOffset-backed implementation and does not govern this replacement. The approved work is split between `docs/superpowers/plans/2026-08-27-relocatable-world-demo-implementation.md` for the minimal standalone demo and `docs/superpowers/plans/2026-08-27-relocatable-world-matrix-implementation.md` for locked six-node evidence.

## 17. Verification and Acceptance

Compile-time tests require:

- all stored region types are standard-layout, trivially copyable, and implicit-lifetime;
- exact size and alignment of `relative_ptr` and the three container descriptors;
- no stored native pointer, reference, allocator, iterator, or standard container;
- `relative_ptr<Entity>` has a finite non-opaque TypeLayout signature;
- ordinary copy rejects same-region representations;
- whole-region relocation admits the four declared payload types;
- the native-pointer alternative is rejected.
- a pointer-free `same_region` fixture whose relocation trait is explicitly disabled remains rejected when wrapped in `region_vector`, proving that dynamic-element Admission is not bypassed by the descriptor.

Runtime tests require:

- region allocation alignment and non-reallocation;
- correct empty and non-empty container validation;
- vector/string/map overflow, bounds, and alignment rejection;
- sorted and unique flat-map keys;
- successful positive relocation, business mutation, and reload;
- the three required negative cases at their intended layers;
- all existing TypeLayout tests remain green.

Matrix acceptance additionally requires:

- six valid node provenance files;
- all six probes report eight-bit bytes, 64-bit pointers, little-endian order, working P2996 reflection, and usable `std::start_lifetime_as` plus `std::start_lifetime_as_array`;
- six signature and six region artifacts in a permitting run;
- one source commit, one workflow invocation, and one committed source/output lock pair across every artifact in the closure;
- exactly 15 Agreement records containing 60 named TypeLayout `PERMIT` decisions;
- exactly 30 directed cross-load records with status `PASS`;
- final workflow closure status `PASS`.

Counts retain node identities and edge directions so duplicates cannot hide omissions.

## 18. Claim Boundary

The demonstrated claim is:

> For this explicitly declared four-type payload contract, a trusted self-contained region produced by any locked 64-bit little-endian node can be relocated as one unit and loaded by any other declared node when every type passes whole-region Admission, producer and consumer TypeLayout signatures agree, the checkpoint envelope and stored ranges validate, and every application relative pointer validates before dereference.

The demo does not prove ordinary-copy admission for same-region pointers, XOffset compatibility, application semantics, schema evolution, arbitrary object relocation, or compatibility with unlisted builds. It demonstrates one explicit relocation profile whose correctness depends on copying the complete region while preserving its region-relative offset space.
