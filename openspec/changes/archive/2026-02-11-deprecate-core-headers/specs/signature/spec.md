## ADDED Requirements

### Requirement: Header Deprecation Notices
The library SHALL emit deprecation warnings when users include headers from
the legacy `core/` directory, directing them to the new root-level equivalents.

#### Scenario: Deprecated core header triggers warning
- **WHEN** a user includes `boost/typelayout/core/fwd.hpp`
- **THEN** a compiler warning SHALL be emitted indicating the header is deprecated
- **AND** the warning SHALL name the replacement header(s)

### Requirement: Migration Documentation
The library SHALL provide a migration guide documenting all breaking or
structural changes, including header path changes, new macro APIs, and
FixedString<N> semantic updates.

#### Scenario: Migration guide covers header changes
- **WHEN** a user reads `docs/migration-guide.md`
- **THEN** every deprecated `core/` header SHALL be listed with its replacement
- **AND** the TYPELAYOUT_REGISTER_TYPES macro SHALL be documented
- **AND** the FixedString<N> semantic change SHALL be explained
