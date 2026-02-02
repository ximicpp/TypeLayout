# Change: Remove Opaque Type Specializations from Core

## Why

The core library's identity is to report **physical layout facts**, not to make semantic decisions about how types should be presented. The current codebase includes template specializations that:

1. **Hide implementation details** of STL types (smart pointers, containers, atomic)
2. **Provide "friendly names"** instead of actual struct layout
3. **Include third-party type support** (boost::interprocess::offset_ptr)

These are **policy decisions** (semantic), not **layout facts** (physical). They should be removed from core to maintain purity.

## What Changes

### Removed from `core/type_signature.hpp`:
- **TypeDiagnostic** utility and related macros/concepts (lines 33-121)
- **std::unique_ptr** specialization (lines 496-505)
- **std::shared_ptr** specialization (lines 508-517)
- **std::weak_ptr** specialization (lines 519-529)
- **std::array** specialization (lines 541-555)
- **std::pair** specialization (lines 567-580)
- **std::span** specialization (lines 588-625)
- **std::atomic** specialization (lines 631-651)
- **boost::interprocess::offset_ptr** specialization (lines 658-676)

### Impact:
- These types will now be processed by the **generic reflection engine**
- Signatures will show actual internal structure (implementation-defined)
- Signatures may vary between libc++ and libstdc++
- **BREAKING**: Existing signatures for these types will change

## Impact

- Affected code: `include/boost/typelayout/core/type_signature.hpp`
- Breaking change: Yes - signatures for smart pointers and STL containers will change
- Rationale: Core should only report what IS in memory, not what we WANT users to see
