# Change: Update signature grammar to cover opaque types

## Why
The signature grammar in the paper (sec3-system.md, section 3.2) and the
supported type table (section 3.5) do not account for opaque type signatures
introduced by TYPELAYOUT_OPAQUE_TYPE / OPAQUE_CONTAINER / OPAQUE_MAP macros.
This means the grammar is no longer complete with respect to the actual
signatures the library can produce.  Lemma 4.7 (Grammar Unambiguity) also
needs re-verification after the grammar extension.

## What Changes
- Add `opaque` and `opaque_tmpl` productions to the Layout grammar (sec3-system.md, 3.2.1)
- The Definition grammar (3.2.2) does not need separate productions because
  opaque signatures are mode-independent (identical in Layout and Definition)
- Add an "Opaque types" row to the supported type table (3.5)
- Add a note to sec4-formal.md about the Opaque Annotation Correctness axiom
- Verify Lemma 4.7 still holds after the extension

## Impact
- Affected specs: documentation
- Affected files: docs/paper/sec3-system.md, docs/paper/sec4-formal.md