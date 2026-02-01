# Change: Add Runtime Verification Examples

## Why
Design analysis revealed that TypeLayout already supports runtime verification, but documentation lacks examples for network/file scenarios. Users may not realize `constexpr` hash values are usable at runtime.

## What Changes
- Add network protocol verification example
- Add file format verification example
- Add best practices guide for runtime scenarios
- Update quickstart with runtime usage patterns

## Impact
- Affected specs: documentation
- Affected code: `doc/`, `example/`
- No API changes required
