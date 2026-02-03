# TypeLayout: Presentation Outline

## Talk Duration Options

| Format | Duration | Depth |
|--------|----------|-------|
| Regular Talk | 60 min | Full technical deep-dive + live demo |
| Short Talk | 30 min | Core concepts + key demo |

---

## 60-Minute Regular Talk Outline

### Part 1: The Problem (10 min)

**1.1 The ABI Horror Story** (5 min)
- Real-world war story: Silent data corruption from struct reordering
- Why this bug class is so dangerous (bypasses all safety nets)
- The "it works on my machine" syndrome in binary interfaces

**1.2 Current Solutions and Their Limitations** (5 min)
- Manual version numbers: Forgotten updates, can't detect field reorder
- IDL-based systems (Protobuf, FlatBuffers): Code generation tax, learning curve
- Runtime RTTI: Not available for layout, only for polymorphism

### Part 2: The Solution - TypeLayout (15 min)

**2.1 Core Concept: Layout Signatures** (5 min)
- Live coding: `get_layout_signature<Point>()`
- Anatomy of a signature: `[64-le]struct[s:8,a:4]{@0[x]:i32[s:4,a:4],...}`
- The guarantee: Identical signature ⟺ Identical memory layout

**2.2 The P2996 Magic** (5 min)
- How static reflection enables this (5-minute P2996 primer)
- Key APIs: `nonstatic_data_members_of`, `offset_of`, `type_of`
- Why this wasn't possible before C++26

**2.3 API Tour** (5 min)
- `get_layout_hash<T>()` - 64-bit FNV-1a for fast comparison
- `get_layout_verification<T>()` - Dual-hash for maximum collision resistance
- Concepts: `LayoutSupported`, `LayoutCompatible`, `LayoutMatch`

### Part 3: Production Patterns (20 min)

**3.1 Pattern: Shared Memory IPC** (5 min)
- Problem: Process A and B must agree on struct layout
- Solution: Embed hash in shared memory header
- Live demo: Detecting mismatch before corruption

**3.2 Pattern: Network Protocols** (5 min)
- Problem: Zero-copy parsing requires layout agreement
- Solution: Layout hash in packet header
- Comparison with Protobuf/FlatBuffers/Cap'n Proto

**3.3 Pattern: Plugin/DLL Systems** (5 min)
- Problem: Host and plugin compiled separately
- Solution: Interface struct with embedded hashes
- **LIVE DEMO**: Loading compatible vs incompatible plugins

**3.4 Pattern: Binary File Formats** (5 min)
- Problem: File format evolves over versions
- Solution: Layout hash in file header for automatic detection
- Migration strategies for schema evolution

### Part 4: Deep Dive - How It Works (10 min)

**4.1 Recursive Signature Generation** (5 min)
- Handling nested structs
- Inheritance and polymorphism
- Bit-field precision: `@4.2[flags]:bits<3,u8>`

**4.2 Platform Awareness** (3 min)
- Architecture prefix: `[64-le]` vs `[32-be]`
- Platform-dependent types: `wchar_t`, `long`
- Cross-platform serialization safety

**4.3 Performance Considerations** (2 min)
- Compile-time only: Zero runtime cost
- `constexpr` step limits for large types
- Binary size: No impact (no template bloat)

### Part 5: Q&A + Future Directions (5 min)

- Boost library submission roadmap
- Potential C++ standard library inclusion
- Community feedback and contributions

---

## 30-Minute Short Talk Outline

### Part 1: The Problem (5 min)
- ABI horror story (condensed)
- Why current solutions fall short

### Part 2: TypeLayout Solution (10 min)
- Live coding: Signature generation
- 3-minute P2996 primer
- Core API demonstration

### Part 3: Key Pattern Demo (10 min)
- **LIVE DEMO**: Plugin system with compatible/incompatible plugins
- Brief mention of other patterns

### Part 4: Wrap-up (5 min)
- Key takeaways
- How to get started
- Q&A

---

## Live Demo Plan

### Demo Environment
- Docker container with Bloomberg Clang P2996
- VS Code with terminal
- Pre-built examples ready to run

### Demo 1: Signature Exploration (3 min)
```cpp
struct Point { int32_t x, y; };
std::cout << get_layout_signature<Point>() << "\n";
// Show signature breakdown
```

### Demo 2: Plugin System (5 min)
```bash
# Run plugin_interface_example
./plugin_interface_example

# Show output:
# - Compatible plugin loads successfully
# - Incompatible plugin rejected with hash mismatch
```

### Demo 3 (if time): Break Something (2 min)
```cpp
// Live edit: Add a field to struct
// Recompile: Watch static_assert fail
// Message: "ABI contract violated at compile time!"
```

---

## Slides Estimate

| Section | Slides |
|---------|--------|
| Title + Agenda | 2 |
| The Problem | 5 |
| The Solution | 8 |
| Production Patterns | 10 |
| Deep Dive | 6 |
| Q&A + Future | 2 |
| **Total** | **~33 slides** |

---

## Backup Material

- Complex type support (variant, optional, tuple)
- Anonymous member handling
- Compiler step limit tuning
- Comparison table with alternatives
