# Design: Boost Review Readiness

## Context

TypeLayout is preparing for formal Boost library review. Analysis of historical Boost mailing list discussions (boost-describe, boost-parser, boost-json, boost-hash2, boost-scope, boost-optional, boost-mp11) has revealed recurring reviewer concerns that must be proactively addressed.

### Key Findings from Mailing List Analysis

| Source Library | Critical Feedback | TypeLayout Implication |
|---------------|-------------------|----------------------|
| boost-describe | "5-minute rule" - docs must show value immediately | Restructure README |
| boost-parser | 1000x perf penalty from stringstream in "off" tracing | Avoid any expensive constructs |
| boost-hash2 | `result()` vs `finalize()` naming debate | Use clear verb-noun API names |
| boost-scope | "Breaking API is unacceptable" | Define stability policy |
| boost-json | "Why not just use std::?" | Document C++26 positioning |
| boost-mp11 | Module/header coexistence issues | Plan module support |

## Goals / Non-Goals

### Goals
1. Ensure documentation conveys value proposition within 5 minutes
2. Prove zero-overhead abstraction with benchmarks
3. Provide complete, runnable use case examples
4. Define clear API stability guarantees
5. Position library relative to C++ standard (P2996)

### Non-Goals
1. Adding new functional features before review
2. Supporting pre-C++26 compilers (core reflection requires C++26)
3. Achieving feature parity with Boost.Describe (different problem space)

## Decisions

### Decision 1: Constexpr-Only Hash Implementation
**What**: All hash algorithms (FNV-1a, DJB2) must be fully constexpr with no runtime dependencies.

**Why**: The boost-parser review showed that even "disabled" tracing using std::stringstream caused 1000x overhead due to std::locale construction. TypeLayout must guarantee true zero-overhead.

**Implementation**:
```cpp
// CORRECT: Pure constexpr, no allocations
[[nodiscard]] constexpr uint64_t fnv1a_hash(std::string_view input) noexcept {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : input) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// WRONG: Would incur runtime cost
std::string format_type_info() { 
    std::ostringstream oss;  // NEVER do this
    // ...
}
```

### Decision 2: Comparison-First Documentation
**What**: README must open with problem statement and comparison table, not feature list.

**Why**: boost-describe feedback emphasized that reviewers want to understand "what problem does this solve" before technical details.

**Structure**:
```markdown
# Boost.TypeLayout

> Verify that types have identical memory layouts at compile-time.

## The Problem
When sharing data between processes, plugins, or network endpoints...

## The Solution (One-Liner)
```cpp
static_assert(get_layout_signature<MyType>() == expected_signature);
```

## Why TypeLayout vs Alternatives
| Feature | TypeLayout | Boost.Describe | Boost.PFR |
|---------|-----------|----------------|-----------|
| Requires macros | No | Yes | No |
| Layout hash | Yes | No | No |
| ...
```

### Decision 3: Version Macro Strategy
**What**: Add `BOOST_TYPELAYOUT_VERSION` macro with semantic versioning.

**Why**: boost-scope discussion highlighted the importance of clear versioning for API stability expectations.

```cpp
#define BOOST_TYPELAYOUT_VERSION 100000  // 1.0.0 = 1*100000 + 0*100 + 0
#define BOOST_TYPELAYOUT_VERSION_MAJOR 1
#define BOOST_TYPELAYOUT_VERSION_MINOR 0
#define BOOST_TYPELAYOUT_VERSION_PATCH 0
```

### Decision 4: [[nodiscard]] on All Signature Functions
**What**: All functions returning signatures must be marked `[[nodiscard]]`.

**Why**: boost-hash2 discussion noted that pure accessor functions should use naming/attributes that prevent misuse.

```cpp
[[nodiscard]] constexpr LayoutSignature get_layout_signature() noexcept;
```

## Risks / Trade-offs

### Risk: C++26 Compiler Availability
**Concern**: C++26 with P2996 support is not yet widely available.
**Mitigation**: Document that this is a forward-looking library. Provide CI configuration for experimental compiler flags.

### Risk: Benchmark Methodology Disputes
**Concern**: Reviewers may question compile-time benchmark accuracy.
**Mitigation**: Use standardized benchmark methodology (Nonius/Google Benchmark for runtime, template instantiation counts for compile-time). Document methodology transparently.

### Trade-off: Documentation Size vs Depth
**Decision**: Prioritize concise README with links to deeper documentation.
**Rationale**: The "5-minute rule" requires brevity, but complete examples are still essential.

## Resolved Questions

1. **Module Support Priority**: ✅ **Post-review enhancement**. Focus on core functionality for initial review. C++20 module support will be added as a follow-up after acceptance.

## Open Questions

1. **Example Complexity**: How complex should the IPC/Plugin examples be? Minimal working examples vs production-ready patterns?

2. **Benchmark Baseline**: Which alternative libraries should be included in compile-time benchmarks? Just Boost.Describe, or also third-party reflection libraries?
