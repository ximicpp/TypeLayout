Delete the build directory and reconfigure CMake from scratch for the TypeLayout project.

Use this when:
- CMakeLists.txt has changed
- Build cache is stale or corrupted
- Build fails with inexplicable errors

## Platform Detection

- **Windows**: Use WSL
- **macOS**: Try local clang with P2996 first, fallback to Docker

## Windows (WSL)

```bash
# Delete old build directory
wsl -e bash -c 'cd /mnt/g/workspace/TypeLayout && rm -rf build'

# Reconfigure
wsl -e bash -c 'cd /mnt/g/workspace/TypeLayout && \
  export CXX=/root/clang-p2996-install/bin/clang++ && \
  cmake -B build \
    -DCMAKE_CXX_COMPILER=$CXX \
    -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++"'
```

## macOS

```bash
rm -rf build

# Try local P2996 clang
clang++ -std=c++26 -freflection -x c++ -c /dev/null -o /dev/null 2>/dev/null && P2996=1 || P2996=0

if [ "$P2996" = "1" ]; then
  cmake -B build -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -freflection-latest -stdlib=libc++"
else
  echo "P2996 clang not found locally. Use Docker or install Bloomberg Clang P2996."
fi
```

## Steps

1. Detect platform
2. Confirm with the user before deleting the build directory
3. Delete `build/`
4. Run cmake configure
5. Report success or failure
