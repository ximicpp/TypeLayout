## 1. Analysis (Complete)
- [x] 1.1 Analyze correctness of V1 guarantee (layout_sig match ⟹ memcmp-compatible)
- [x] 1.2 Identify false positive scenarios (bit-field, padding, float representation)
- [x] 1.3 Analyze Two-Phase Pipeline architecture rationality
- [x] 1.4 Compare alternative approaches (protobuf, FlatBuffers, pahole, manual static_assert)
- [x] 1.5 Analyze real-world user stories (IPC, network protocol, file format, plugin system)

## 2. Documentation Enhancement (P1)
- [ ] 2.1 Add "Correctness Boundary" section to tools/README.md
- [ ] 2.2 Add "Correctness Boundary" section to example/README.md
- [ ] 2.3 Add safety assumptions to CompatReporter output header

## 3. Safety Classification (P2)
- [ ] 3.1 Define safety level enum/categories in sig_types.hpp or compat_check.hpp
- [ ] 3.2 Add safety level to CompatReporter output per type
- [ ] 3.3 Parse signature strings to detect unsafe patterns (ptr, bit-field markers)

## 4. Update Specs
- [ ] 4.1 Update signature spec to formally document V1 correctness boundary
