# Contributing to Boost.TypeLayout

Thank you for your interest in contributing to Boost.TypeLayout! This document provides guidelines and instructions for contributing.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Environment](#development-environment)
- [Code Style](#code-style)
- [Pull Request Process](#pull-request-process)
- [Reporting Issues](#reporting-issues)

## Code of Conduct

This project follows the [Boost Community Guidelines](https://www.boost.org/community/). Please be respectful and constructive in all interactions.

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/TypeLayout.git
   cd TypeLayout
   ```
3. **Add upstream remote**:
   ```bash
   git remote add upstream https://github.com/ximicpp/TypeLayout.git
   ```

## Development Environment

### Requirements

- **Compiler**: Clang with P2996 reflection support (`-freflection -freflection-latest`)
- **CMake**: 3.14 or higher
- **C++ Standard**: C++26

### Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_CXX_FLAGS="-freflection -freflection-latest"
cmake --build .
```

### Running Tests

```bash
# Build tests
cmake --build . --target all

# Run tests with CTest
ctest --output-on-failure
```

## Code Style

### Naming Conventions

| Element | Style | Example |
|---------|-------|---------|
| Functions | `snake_case` | `get_type_signature()` |
| Types/Classes | `snake_case` | `type_info`, `type_descriptor` |
| Macros | `BOOST_TYPELAYOUT_*` | `BOOST_TYPELAYOUT_VERSION` |
| Template Parameters | `PascalCase` | `typename T`, `typename Platform` |
| Constants | `snake_case` | `default_platform` |

### Include Guards

Use the pattern `BOOST_TYPELAYOUT_<PATH>_<FILENAME>_HPP`:

```cpp
#ifndef BOOST_TYPELAYOUT_CORE_TYPE_SIGNATURE_HPP
#define BOOST_TYPELAYOUT_CORE_TYPE_SIGNATURE_HPP
// ...
#endif // BOOST_TYPELAYOUT_CORE_TYPE_SIGNATURE_HPP
```

### File Headers

All source files must include the Boost Software License header:

```cpp
// Copyright (c) 2025 Ximi Zhong
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
```

### Code Formatting

- Use 4-space indentation (no tabs)
- Maximum line length: 100 characters
- Opening braces on the same line
- Add `noexcept` to functions that don't throw

## Pull Request Process

### Before Submitting

1. **Create a feature branch**:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes** following the code style guidelines

3. **Add tests** for new functionality

4. **Ensure all tests pass**:
   ```bash
   ctest --output-on-failure
   ```

5. **Update documentation** if needed

### Submitting

1. **Push your branch**:
   ```bash
   git push origin feature/your-feature-name
   ```

2. **Create a Pull Request** on GitHub

3. **Fill out the PR template** completely

4. **Wait for review** and address any feedback

### PR Requirements

- [ ] All tests pass
- [ ] Code follows style guidelines
- [ ] Documentation updated (if applicable)
- [ ] CHANGELOG.md updated (for user-facing changes)
- [ ] Commit messages are clear and descriptive

## Reporting Issues

### Bug Reports

When reporting a bug, please include:

1. **Environment**: Compiler version, OS, CMake version
2. **Steps to reproduce**: Minimal code example
3. **Expected behavior**: What you expected to happen
4. **Actual behavior**: What actually happened
5. **Error messages**: Any compiler errors or runtime output

### Feature Requests

When requesting a feature, please include:

1. **Use case**: Why do you need this feature?
2. **Proposed solution**: How would you like it to work?
3. **Alternatives**: Have you considered other approaches?

## Questions?

If you have questions, feel free to:

- Open a [Discussion](https://github.com/ximicpp/TypeLayout/discussions)
- Ask in the [Boost Developers mailing list](https://www.boost.org/community/groups.html)

Thank you for contributing! 🎉
