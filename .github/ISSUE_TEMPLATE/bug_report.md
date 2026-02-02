---
name: Bug Report
about: Report a bug or unexpected behavior
title: '[BUG] '
labels: bug
assignees: ''
---

## Description

A clear and concise description of the bug.

## Environment

- **OS**: [e.g., Ubuntu 22.04, Windows 11]
- **Compiler**: [e.g., Clang P2996 (commit hash)]
- **CMake Version**: [e.g., 3.26]
- **TypeLayout Version**: [e.g., 0.1.0]

## Steps to Reproduce

```cpp
// Minimal code example that reproduces the issue
#include <boost/typelayout.hpp>

struct Example {
    int x;
    double y;
};

int main() {
    // Code that triggers the bug
}
```

## Expected Behavior

What you expected to happen.

## Actual Behavior

What actually happened. Include any error messages:

```
// Compiler errors or runtime output
```

## Additional Context

Add any other context about the problem here.
