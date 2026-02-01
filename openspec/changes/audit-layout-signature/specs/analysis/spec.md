# Layout Signature Audit Spec Delta

## ADDED Requirements

### Requirement: Comprehensive Type Coverage

The layout signature system SHALL support generating signatures for all standard C++ types including:

1. **Fundamental Types**
   - Fixed-width integers (int8_t through int64_t, unsigned variants)
   - Floating point types (float, double, long double)
   - Character types (char, wchar_t, char8_t, char16_t, char32_t)
   - Boolean and special types (bool, std::byte, std::nullptr_t)

2. **Compound Types**
   - Pointer types (T*, void*, function pointers)
   - Reference types (T&, T&&)
   - Member pointer types (T C::*, member function pointers)
   - Array types (T[N], multidimensional arrays)
   - CV-qualified types (const, volatile)

3. **User-Defined Types**
   - Empty structs
   - POD structs
   - Classes with inheritance (single, multiple, virtual)
   - Polymorphic classes
   - Unions
   - Enums (C-style and scoped)
   - Bit-fields

4. **Standard Library Types**
   - Smart pointers (unique_ptr, shared_ptr, weak_ptr)

#### Scenario: All fundamental types generate signatures
- **WHEN** calling `get_layout_signature<T>()` for any fundamental type
- **THEN** a valid signature string is returned
- **AND** compilation succeeds without errors

#### Scenario: Complex user-defined types generate signatures
- **WHEN** calling `get_layout_signature<T>()` for a class with virtual inheritance
- **THEN** a valid signature is returned
- **AND** the signature includes base class layout information

#### Scenario: Bit-fields are correctly represented
- **WHEN** calling `get_layout_signature<T>()` for a struct with bit-fields
- **THEN** the signature includes bit offset and bit width information
- **AND** the format follows `@byte.bit[name]:bits<width,type>`

### Requirement: Edge Case Handling

The layout signature system SHALL correctly handle edge cases including:
- Anonymous unions/structs within types
- Types with `[[no_unique_address]]` attribute
- Types with custom alignment (`alignas`)
- Packed structures (`__attribute__((packed))` or `#pragma pack`)
- Nested template instantiations

#### Scenario: Aligned struct signature includes alignment
- **WHEN** a struct uses `alignas(16)` attribute
- **THEN** the signature reflects the actual alignment value
- **AND** size includes any padding from alignment

#### Scenario: Packed struct signature reflects no padding
- **WHEN** a struct is marked as packed
- **THEN** the signature size equals sum of member sizes
- **AND** alignment is 1

### Requirement: Signature Correctness Verification

Each generated signature SHALL accurately reflect the actual memory layout such that:
- `sizeof(T)` matches the size in the signature
- `alignof(T)` matches the alignment in the signature
- Member offsets match actual `offsetof()` values
- Identical layout types produce identical signatures

#### Scenario: Size and alignment match runtime values
- **WHEN** signature claims `[s:X,a:Y]` for type T
- **THEN** `sizeof(T) == X` is true
- **AND** `alignof(T) == Y` is true

#### Scenario: Different layouts produce different signatures
- **WHEN** two types have different memory layouts
- **THEN** their signatures are different
- **AND** `signatures_match<T1, T2>()` returns false
