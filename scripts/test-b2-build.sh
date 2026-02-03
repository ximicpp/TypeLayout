#!/bin/bash
# B2 Build Test Script
# Run inside P2996 Docker container

set -e

echo "=== Installing B2 ==="
cd /tmp
if [ ! -d "b2" ]; then
    git clone --depth 1 -q https://github.com/boostorg/build.git b2
fi
cd b2
./bootstrap.sh > /dev/null 2>&1
./b2 install --prefix=/usr/local > /dev/null 2>&1
echo "B2 installed: $(b2 --version 2>&1 | head -1)"

echo ""
echo "=== Configuring B2 Toolset ==="
cat > ~/user-config.jam << 'JAMEOF'
using clang : p2996 : clang++
    : <cxxflags>"-std=c++26 -freflection -freflection-latest -stdlib=libc++"
      <linkflags>"-stdlib=libc++ -L/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu -Wl,-rpath,/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu"
    ;
JAMEOF
echo "Toolset configured"

echo ""
echo "=== Building Examples ==="
cd /workspace
export LD_LIBRARY_PATH=/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH
b2 toolset=clang-p2996 example -j4

echo ""
echo "=== Running Tests ==="
b2 toolset=clang-p2996 test -j4

echo ""
echo "=== Build Complete ==="
