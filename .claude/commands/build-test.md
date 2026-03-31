Build and test the TypeLayout project. Detect the current platform and use the appropriate method.

## Platform Detection

- **Windows**: Use WSL (preferred) or Docker
- **macOS**: Try local P2996-capable compiler first, fallback to Docker
- **Docker**: GCC 16 (recommended) or Bloomberg Clang (legacy)

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

## macOS (local compiler or Docker)

First check if a local P2996-capable compiler is available:

```bash
# Try GCC 16 first
g++-16 -std=c++26 -freflection -x c++ -c /dev/null -o /dev/null 2>/dev/null && echo "GCC16_OK" || echo "GCC16_MISSING"

# Try Clang with P2996
clang++ -std=c++26 -freflection -x c++ -c /dev/null -o /dev/null 2>/dev/null && echo "CLANG_P2996_OK" || echo "CLANG_P2996_MISSING"
```

If GCC16_OK, use GCC directly:

```bash
cmake -B build -DCMAKE_CXX_COMPILER=g++-16
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

If CLANG_P2996_OK, use local clang:

```bash
cmake -B build -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++"
cmake --build build -j$(sysctl -n hw.ncpu)
ctest --test-dir build --output-on-failure
```

If neither available, fall back to Docker (see below).

## Docker with GCC 16 (recommended)

```bash
docker run --rm --platform linux/amd64 \
  -v $(pwd):/workspace -w /workspace \
  sourcemation/gcc-16 \
  bash -c 'apt-get update -qq && apt-get install -y -qq cmake > /dev/null 2>&1 && \
  cmake -B build -DCMAKE_CXX_COMPILER=g++ && \
  cmake --build build -j$(nproc) && \
  ctest --test-dir build --output-on-failure'
```

## Docker with Bloomberg Clang (legacy fallback)

```bash
docker run --rm --platform linux/amd64 \
  -v $(pwd):/workspace -w /workspace \
  -e LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu \
  ghcr.io/ximicpp/typelayout-p2996:latest \
  bash -c 'cmake -B build \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++" && \
  cmake --build build -j$(nproc) && \
  ctest --test-dir build --output-on-failure'
```

## Key Notes

- **GCC 16**: uses `-freflection`, libstdc++, `-fconstexpr-ops-limit`
- **Bloomberg Clang**: uses `-freflection -freflection-latest -stdlib=libc++`, `-fconstexpr-steps`
- CMakeLists.txt auto-detects the compiler and sets flags accordingly
- All 12 tests must pass: 7 core + 5 tools
- If build fails, check if CMake cache is stale: delete `build/` and reconfigure
- Git push must be done from Windows (WSL lacks git credentials): `git push origin main`

## Steps

1. Detect platform (check if `wsl` command exists for Windows, check for Docker)
2. Build with cmake (prefer GCC 16, fallback to Bloomberg Clang)
3. Run ctest
4. Report results
