# Change: Fix Layout engine bypassing opaque TypeSignature specializations

## Why
The Layout signature engine unconditionally flattens all class-type fields
by recursing into their members. This bypasses TypeSignature specializations
registered via TYPELAYOUT_OPAQUE_* macros, causing opaque types (e.g.,
shared-memory containers) to be expanded into their internal byte layout
instead of being emitted as opaque leaf nodes.

## What Changes
- Add `is_opaque` static marker to all three opaque macro expansions
- Add `has_opaque_signature` concept to detect opaque specializations at compile time
- Guard the recursive flattening branch in `layout_field_with_comma` so that
  opaque class types are emitted as leaf nodes
- Enable previously disabled integration tests (Tests 12-13) in test_opaque.cpp

## Impact
- Affected specs: signature
- Affected code: `core/opaque.hpp`, `core/signature_detail.hpp`, `test/test_opaque.cpp`
