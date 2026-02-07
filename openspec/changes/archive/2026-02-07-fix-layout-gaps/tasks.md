## 1. P2 Fix
- [x] 1.1 Add `R(*)(Args..., ...) noexcept` function pointer specialization

## 2. Missing Layout tests
- [x] 2.1 Pointer and reference types (`int*`, `int&`, `int&&`)
- [x] 2.2 Function pointer types
- [x] 2.3 Platform-dependent types (`long double`, `wchar_t`)
- [x] 2.4 Anonymous member Layout behavior
- [x] 2.5 Multidimensional array `T[M][N]` and struct with array field
- [x] 2.6 CV-qualified fields (`const int`, `volatile int`)

## 3. Validation
- [x] 3.1 Build and run tests in Docker container