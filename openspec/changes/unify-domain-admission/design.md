## Context

TypeLayout and XOffset share a common question: "is type T safe for byte-copy into shared/persistent memory?" TypeLayout answers a strict subset via `is_local_serialization_free_v<T>` (trivially_copyable + no pointer). XOffset extends this domain with offset_ptr-based containers (XString, XVector, etc.) that are NOT trivially_copyable but ARE byte-copy safe, duplicating TypeLayout's P2996 reflection infrastructure to build its own recursive admission gate.

The proposal established that TypeLayout should own a unified recursive admission predicate (`is_byte_copy_safe_v<T>`) that subsumes XOffset's `DefaultPolicy::accept<T>()`. This design document records the technical decisions made during exploration and defines the implementation approach.

### Current state

- TypeLayout has 3 independent P2996 recursive traversals (signature generation, `type_has_opaque`, `compute_has_padding`)
- XOffset duplicates a 4th traversal for admission checking (`accept_all_members_impl` / `accept_all_bases_impl`)
- XOffset's external TypeLayout copy has already diverged (has `SignatureMode` parameter; TypeLayout main does not)
- `is_local_serialization_free_v<T>` is a flat, whole-type check -- rejects any type where the top-level struct is not trivially_copyable

## Goals / Non-Goals

**Goals:**

- Provide `is_byte_copy_safe_v<T>` as a first-class TypeLayout API -- recursive, per-member, opaque-aware admission predicate
- Provide `opaque_elements_safe<T>` as a customization point for opaque container/map element type safety
- Enable XOffset to delete all duplicated P2996 traversal and judgment logic
- Maintain full backward compatibility with all existing TypeLayout APIs and tests

**Non-Goals:**

- Changing `is_local_serialization_free_v<T>` semantics (preserved as-is)
- Changing `SafetyLevel` enum or `classify<T>` (orthogonal; relationship documented only)
- Adding diagnostic/error-location features (XOffset deletes `diagnose_unsafe_members`; no TypeLayout replacement)
- Adding a `is_byte_copy_transfer_safe` variant (opaque containers are not suitable for cross-platform transfer)
- Providing a policy framework (DefaultPolicy/StrictPolicy/SmallTypePolicy all deleted from XOffset; no TypeLayout replacement)
- Abstracting a generic P2996 visitor (premature; 4 traversals have sufficiently different leaf logic)

## Decisions

### D1: Naming -- `is_byte_copy_safe_v<T>`

**Decision:** Use `is_byte_copy_safe` / `is_byte_copy_safe_v<T>`.

**Alternatives considered:**
- `is_relocatable_safe` -- rejected: conflicts with P1144 (`trivially_relocatable`) terminology
- `is_opaque_domain_safe` -- rejected: meaningless to users unfamiliar with TypeLayout's opaque system

**Rationale:** "byte copy" precisely describes the operation (memcpy / byte-level copy) and forms a clear conceptual pair with `is_local_serialization_free`.

### D2: Decision tree -- polymorphic rejection is a correctness requirement

**Decision:** Branch 3 of the decision tree MUST include `!is_polymorphic_v<T>`.

```
is_byte_copy_safe_v<T> =
  | has_opaque_signature<T>                    -> !has_pointer
  |                                               && opaque_elements_safe<T>
  | is_local_serialization_free_v<T>           -> true
  | is_class && !union && !is_polymorphic_v<T> -> all bases byte_copy_safe
  |                                               && all members byte_copy_safe
  | otherwise                                  -> false
```

**Rationale:** P2996's `nonstatic_data_members_of` does NOT expose the vptr. A polymorphic type's vptr is a compiler-inserted implicit pointer that occupies real bytes but is invisible to reflection. Without the `!is_polymorphic_v<T>` guard, the recursive member check would return `true` for a polymorphic struct whose visible members are all safe -- incorrectly, because the vptr is a runtime address dependency that makes byte-copy unsafe.

This is a **correctness requirement**, not a performance optimization. The original proposal listed it as an open question; this design closes it.

### D3: Union handling -- no special logic needed

**Decision:** No special union handling code. The decision tree naturally handles unions correctly.

**How it works:**
- POD unions (trivially_copyable, no pointer) are accepted by Branch 2 (`is_local_serialization_free`)
- Non-trivial unions (e.g., containing opaque members) fall through Branch 3's `!union` condition to Branch 4 (`false`)
- Unions as struct members: recursion applies `is_byte_copy_safe` to the union type, which follows the same rules

**Rationale:** Union + non-trivial member (e.g., XString) requires manual lifetime management in C++ and is unlikely to appear in shared memory scenarios. The conservative rejection is correct.

### D4: `opaque_elements_safe<T>` -- default false, macros generate specializations

**Decision:** Default trait returns `false`. Each opaque registration macro generates an explicit specialization.

| Macro | Generated `opaque_elements_safe` value |
|-------|---------------------------------------|
| `TYPELAYOUT_REGISTER_OPAQUE` | `true` (no element types) |
| `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE` | `true` (no element types) |
| `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE<T>` | `is_byte_copy_safe_v<T_>` |
| `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE<K,V>` | `is_byte_copy_safe_v<K_> && is_byte_copy_safe_v<V_>` |
| Hand-written `TypeSignature` with `is_opaque=true` | Default `false` (must specialize explicitly) |

**Rationale:** Conservative default prevents silent false-positives when users hand-write opaque specializations without declaring element safety.

### D5: Branch 1 dual check -- `!has_pointer && opaque_elements_safe`

**Decision:** Branch 1 (opaque types) requires BOTH `!has_pointer` AND `opaque_elements_safe`.

**Rationale:** The two checks guard different things:
- `!has_pointer`: guards the container's own internal structure (from user-declared `pointer_free` or signature scanning)
- `opaque_elements_safe`: guards the element types via recursive `is_byte_copy_safe`

They are not redundant. A container could be pointer-free itself but hold elements that contain pointers. Conversely, a container could hold safe elements but internally use a raw pointer for bookkeeping (incorrectly registered -- but that is the opaque contract: user declares, user is responsible).

### D6: File placement -- new `admission.hpp`, trait default in `fwd.hpp`

**Decision:**
- New file `include/boost/typelayout/admission.hpp` for `is_byte_copy_safe<T>` / `is_byte_copy_safe_v<T>`
- `opaque_elements_safe<T>` default trait (`struct opaque_elements_safe : std::false_type {}`) in `fwd.hpp`
- Opaque macros in `opaque.hpp` generate specializations of `opaque_elements_safe`

**Alternatives considered:**
- Put everything in `layout_traits.hpp` -- rejected: 310+ lines already, different semantic concern (structural properties vs. admission judgment)

**Dependency chain (no cycles):**
```
fwd.hpp                  (opaque_elements_safe default trait)
   ^
   |
opaque.hpp               (macros generate opaque_elements_safe specializations)
   ^                      (already includes fwd.hpp via fixed_string.hpp chain)
   |
layout_traits.hpp        (has_pointer, has_opaque_signature)
   ^
   |
admission.hpp [NEW]      (is_byte_copy_safe -- uses has_pointer + opaque_elements_safe)
   ^
   |
typelayout.hpp           (umbrella, includes admission.hpp)
```

### D7: SafetyLevel relationship -- orthogonal, document only

**Decision:** `SafetyLevel` enum and `classify<T>` are unchanged. The relationship between `SafetyLevel::Opaque` and `is_byte_copy_safe_v<T> == true` is documented in comments only.

**Rationale:**
- `SafetyLevel` measures "how much external trust is required" -- a property of the type's analyzability
- `is_byte_copy_safe` measures "given that trust, is the type safe" -- a conclusion given user declarations
- These are orthogonal dimensions; coupling them would break the existing 5-tier design used across `classify.hpp`, `safety_level.hpp`, `compat_check.hpp`, and two C++17 tests

### D8: XOffset cleanup scope -- maximum deletion

**Decision:** After this change, XOffset deletes ALL of the following:

| Deleted from XOffset | Replacement |
|---------------------|-------------|
| `DefaultPolicy` / `StrictPolicy` / `SmallTypePolicy` | Direct `static_assert(is_byte_copy_safe_v<T>, ...)` |
| `accept_all_members_impl` | Subsumed by `is_byte_copy_safe` recursion |
| `accept_all_bases_impl` | Subsumed by `is_byte_copy_safe` recursion |
| `opaque_element_types<T>` + specializations | Subsumed by `opaque_elements_safe<T>` |
| `diagnose_unsafe_members` | No replacement (DX feature, not correctness) |
| `XOFFSET_REGISTER_*` element-check clauses | Subsumed by macro-generated `opaque_elements_safe` |

XOffset retains ONLY:
- Data structure implementations (XString, XVector, XMap, offset_ptr)
- Type registrations (calls to `TYPELAYOUT_OPAQUE_*_RELOCATABLE` macros)
- `XCompactor::migrate_as` (orthogonal concern)

### D9: `is_transfer_safe` -- no changes

**Decision:** `is_transfer_safe<T>(remote_sig)` is unchanged. No `is_byte_copy_transfer_safe` variant is added.

**Rationale:**
- Local shared memory: needs only `is_byte_copy_safe_v<T>`, no signature comparison
- Cross-platform transfer: opaque containers have implementation-defined internal layout (offset_ptr specifics, allocator metadata); signature match does not guarantee binary compatibility across platforms
- `is_transfer_safe` correctly rejects non-trivially-copyable types; this is the right behavior for cross-platform scenarios

## Risks / Trade-offs

### [Risk] Fourth P2996 recursive traversal

TypeLayout will have 4 independent P2996 member/base traversals (signature generation, `type_has_opaque`, `compute_has_padding`, `is_byte_copy_safe`). Each has similar structure but different leaf logic.

Mitigation: Accept for now. The traversals share boilerplate but differ significantly in return types (`FixedString` vs `bool` vs `void+bitmap`). Extracting a generic visitor would add `consteval` template metaprogramming complexity disproportionate to the benefit. Revisit if a 5th or 6th traversal is needed.

### [Risk] Circular dependency between `admission.hpp` and `opaque.hpp`

`admission.hpp` needs `opaque_elements_safe<T>` specializations (generated by macros in `opaque.hpp`). `opaque.hpp` macros need the default trait declaration.

Mitigation: Resolved by placing the default trait in `fwd.hpp`. The specializations in `opaque.hpp` and the consumption in `admission.hpp` are independent compilation units connected only through the user's include order: opaque registration headers before `admission.hpp` (or via `typelayout.hpp` umbrella).

### [Risk] Users forget to register opaque types before using `is_byte_copy_safe`

If an opaque type is not registered, `has_opaque_signature<T>` is false, `is_local_serialization_free_v<T>` is false (non-trivially-copyable), and the recursive check enters Branch 3 which traverses internal members -- potentially seeing raw pointers or other unsafe internals.

Mitigation: This is the existing opaque registration ordering invariant ("Opaque types must be registered BEFORE any signature generation that encounters them"). `is_byte_copy_safe` follows the same rule. No new risk introduced.

### [Trade-off] No diagnostic capability

By deleting `diagnose_unsafe_members` from XOffset and not providing a TypeLayout replacement, users get only a `static_assert` failure with a generic message when `is_byte_copy_safe_v<T>` is false.

Accepted: Diagnostics are a developer experience feature, not a correctness feature. The compiler's template instantiation backtrace provides some indication of which type failed. A diagnostic API can be added later if demand materializes, without affecting the core `is_byte_copy_safe` design.