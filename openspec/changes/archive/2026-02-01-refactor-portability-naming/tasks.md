## 1. API Renaming
- [x] 1.1 Rename `is_portable<T>()` to `is_trivially_serializable<T>()` in `portability.hpp`
- [x] 1.2 Rename `is_portable_v<T>` to `is_trivially_serializable_v<T>`
- [x] 1.3 Rename `Portable<T>` concept to `TriviallySerializable<T>` in `concepts.hpp`
- [x] 1.4 Update forward declaration in `fwd.hpp`
- [x] 1.5 Add deprecated aliases for backward compatibility

## 2. Documentation
- [x] 2.1 Create architecture documentation explaining the layered design
- [x] 2.3 Add migration guide for `is_portable` → `is_trivially_serializable`

## 3. Testing
- [x] 3.1 Update existing test files to use new names
- [x] 3.2 Verify deprecated aliases work correctly
- [x] 3.3 Compile and run tests in WSL