Build and test the TypeLayout project. Detect the current platform and use the appropriate method.

## Platform Detection

- **Windows**: Use WSL (preferred) or Docker
- **macOS**: Try local clang with P2996 first, fallback to Docker

## Windows (WSL)

Bloomberg Clang P2996 is installed in WSL at `/root/clang-p2996-install/`.

```bash
# Configure (only needed once or after CMakeLists.txt changes)
wsl -e bash -c 'cd /mnt/g/workspace/TypeLayout && \
  export CXX=/root/clang-p2996-install/bin/clang++ && \
  cmake -B build \
    -DCMAKE_CXX_COMPILER=$CXX \
    -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++"'

# Build
wsl -e bash -c 'cd /mnt/g/workspace/TypeLayout && \
  export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && \
  cmake --build build -j$(nproc)'

# Test
wsl -e bash -c 'cd /mnt/g/workspace/TypeLayout && \
  export LD_LIBRARY_PATH=/root/clang-p2996-install/lib && \
  ctest --test-dir build --output-on-failure'
```

## macOS (local clang or Docker)

First check if a local P2996-capable clang is available:

```bash
# Detect: try compiling a minimal P2996 test
echo 'int main() { return 0; }' > /tmp/test_p2996.cpp
clang++ -std=c++26 -freflection /tmp/test_p2996.cpp -o /dev/null 2>/dev/null && echo "P2996_OK" || echo "P2996_MISSING"
```

If P2996_OK, use local clang directly:

```bash
# Configure
cmake -B build -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++"

# Build
cmake --build build -j$(sysctl -n hw.ncpu)

# Test
ctest --test-dir build --output-on-failure
```

If P2996_MISSING, fall back to Docker (see below).

## Docker (any platform, fallback)

```bash
docker run --rm -v $(pwd):/workspace -w /workspace \
  -e LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu \
  ghcr.io/ximicpp/typelayout-p2996:latest \
  bash -c 'cmake -B build \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++" && \
  cmake --build build -j$(nproc) && \
  ctest --test-dir build --output-on-failure'
```

## Key Notes

- `LD_LIBRARY_PATH` is required at runtime for the P2996 libc++
- All 10 tests must pass: 5 core + 5 tools
- If build fails, check if CMake cache is stale: delete `build/` and reconfigure
- Git push must be done from Windows (WSL lacks git credentials): `git push origin main`

## Steps

1. Detect platform (check if `wsl` command exists for Windows)
2. Build with cmake
3. Run ctest
4. Report results
