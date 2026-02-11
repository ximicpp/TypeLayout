# Change: Analyze TypeLayout improvements driven by XOffsetDatastructure integration

## Why
XOffsetDatastructure is TypeLayout's primary real-world consumer. After several
rounds of integration (opaque macros, is_fixed_enum, opaque flattening fix),
both projects have stabilized their interface boundary. This is the right time
to perform a systematic gap analysis: what does XOffsetDatastructure actually
need from TypeLayout that is missing, incomplete, or could be better designed?

This proposal is **analysis-only**. It identifies concrete improvement areas and
prioritizes them. Implementation will be handled by follow-up proposals.

## What Changes
- Systematic review of all TypeLayout APIs used by XOffsetDatastructure
- Identify missing capabilities, design friction, and API ergonomic issues
- Evaluate each candidate improvement against the "TypeLayout stays generic" principle
- Produce a prioritized improvement roadmap

## Impact
- Affected specs: signature (potential MODIFIED/ADDED requirements)
- Affected code: core/, tools/, opaque.hpp
- No code changes in this proposal; analysis deliverables only
