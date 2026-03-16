## Context

TypeLayout's `is_transfer_safe<T>(remote_sig)` uses `is_local_serialization_free_v<T>` as its local precondition, requiring `trivially_copyable + no pointer`. This rejects all relocatable opaque types (XVector, XString, etc.) even though `is_byte_copy_safe_v<T>` accepts them. The trust model is inconsistent: byte-copy safety trusts user declarations, but transfer safety does not.

This change aligns `is_transfer_safe` and `SignatureRegistry` with the unified `is_byte_copy_safe_v<T>` predicate introduced in `unify-domain-admission`.

## Goals / Non-Goals

**Goals:**

- Make `is_transfer_safe<T>` use `is_byte_copy_safe_v<T>` as its local precondition
- Make `SignatureRegistry::register_local<T>` accept byte-copy-safe types
- Document the opaque tag matching assumption (user responsibility)
- Maintain full backward compatibility

**Non-Goals:**

- Renaming `SignatureRegistry::is_serialization_free()` (breaking API change; document instead)
- Changing `is_local_serialization_free_v<T>` semantics (preserved as-is)
- Changing `serialization_free_assert<T>` (preserved as strict C++ POD check)
- Modifying opaque signature format

## Decisions

### D1: Trust model consistency

**Decision:** `is_transfer_safe<T>` SHALL use `is_byte_copy_safe_v<T>` as its local precondition instead of `is_local_serialization_free_v<T>`.

**Rationale:** TypeLayout's opaque system operates on a "user declares, user is responsible" trust model. `is_byte_copy_safe_v<T>` already trusts user RELOCATABLE declarations. `is_transfer_safe` must apply the same trust model. A type that is trusted for byte-copy (memcpy within a process) should also be trusted for cross-endpoint transfer when signatures match -- both operations are byte-level copying.

### D2: SignatureRegistry alignment

**Decision:** `SignatureRegistry::register_local<T>` SHALL use `is_byte_copy_safe_v<T>` in its static_assert, replacing `is_local_serialization_free_v<T>`.

**Rationale:** The registry is the runtime counterpart of `is_transfer_safe`. If `is_transfer_safe` accepts opaque types, the registry must also accept them for registration.

### D3: serialization_free_assert preserved

**Decision:** `serialization_free_assert<T>` is NOT changed. It remains a strict C++ standard-level check (trivially_copyable + no pointer).

**Rationale:** Users who need the strict guarantee (e.g., for formal verification or language-level safety proofs) should still have access to it. The assert's name clearly conveys "serialization-free" which is the strict semantic.

### D4: API naming -- document rather than rename

**Decision:** `SignatureRegistry::is_serialization_free(key)` keeps its name. The semantic drift is documented in comments.

**Rationale:** Renaming is a breaking API change. The method's actual behavior (compare local vs remote signatures) is unchanged -- only the set of types that can be registered expands. A comment explains the broader acceptance.

### D5: Opaque tag matching is user's responsibility

**Decision:** Add documentation in `opaque.hpp` explaining that signature matching for opaque types relies on the user's guarantee that same tag + same sizeof + same alignof implies same internal binary layout.

**Rationale:** TypeLayout cannot verify opaque internals by design. The user takes this responsibility when calling `TYPELAYOUT_OPAQUE_*_RELOCATABLE`. This is the existing implicit contract; the documentation makes it explicit.

### D6: Header file comment update

**Decision:** Update `serialization_free.hpp` header comment to reflect that `is_transfer_safe` now accepts byte-copy-safe types (not just trivially_copyable + no pointer).

**Rationale:** The header comment currently states three conditions (trivially_copyable, no pointer, sig match). Condition (1)+(2) are replaced by `is_byte_copy_safe_v<T>` which is a broader predicate. The comment must reflect this.

## Risks / Trade-offs

### [Risk] Opaque containers with same tag but different internal layout

Two endpoints could register different container implementations with the same opaque tag, sizeof, and alignof. Signatures would match but binary layouts would differ, leading to data corruption.

Mitigation: This is inherent to the opaque trust model. Documentation (D5) makes the user's responsibility explicit. This risk already exists for `is_byte_copy_safe_v<T>` -- extending it to `is_transfer_safe` does not introduce a new attack surface.

### [Trade-off] SignatureRegistry naming drift

`is_serialization_free()` now accepts types that are not "serialization-free" in the strict C++ sense. The name is misleading but changing it is a breaking API change.

Mitigation: Document in comments (D4). The method's behavior is unchanged -- it compares signatures. Only the input gate widens.