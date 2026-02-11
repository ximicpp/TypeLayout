## 1. Update FixedString Definition

- [x] 1.1 Change `char value[N]` to `char value[N + 1]`
- [x] 1.2 Change `static constexpr size_t size = N - 1` to `size = N`
- [x] 1.3 Update constructor `FixedString(const char (&str)[N])` to accept `const char (&str)[N + 1]`
- [x] 1.4 Add CTAD deduction guide: `FixedString(const char (&)[N]) -> FixedString<N - 1>`
- [x] 1.5 Update `FixedString(std::string_view sv)` -- loop bounds from `N - 1` to `N`
- [x] 1.6 Update `operator+` -- result size from `N + M - 1` to `N + M`; loop bounds accordingly
- [x] 1.7 Update `operator==` -- comparison bounds from `N`/`M` to `N + 1`/`M + 1`
- [x] 1.8 Update `length()` -- scan limit from `N` to `N + 1` (or just `N` since `value[N]` is always '\0')
- [x] 1.9 Update `operator string_view` -- use updated `length()`
- [x] 1.10 Update `skip_first()` -- loop bounds from `N` to `N + 1`; return type stays `FixedString<N>`

## 2. Update to_fixed_string

- [x] 2.1 Change return type from `FixedString<21>` to `FixedString<20>`
- [x] 2.2 Update internal buffer sizes and index arithmetic accordingly

## 3. Update detail/reflect.hpp

- [x] 3.1 Change `FixedString<self.size() + 1>(self)` to `FixedString<self.size()>(self)` (2 sites)
- [x] 3.2 Change `FixedString<NameLen>(name)` where `NameLen = name.size() + 1` to `FixedString<name.size()>(name)`

## 4. Update Tests

- [x] 4.1 Update `test_fixed_string.cpp` assertions for new `N` semantics
- [x] 4.2 Run all tests -- signature values must be identical to pre-change