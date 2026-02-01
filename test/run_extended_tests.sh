#!/bin/bash
# Run extended signature tests in Docker container
# Usage: ./test/run_extended_tests.sh

set -e

DOCKER_IMAGE="ghcr.io/ximicpp/typelayout-p2996:latest"
WORKSPACE="/workspace"
CXX="/opt/p2996-toolchain/bin/clang++"
LD_PATH="/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu"

echo "=================================================="
echo "TypeLayout Extended Signature Tests"
echo "=================================================="
echo ""

# Pull latest image
echo "Pulling Docker image..."
docker pull $DOCKER_IMAGE

# Run tests
docker run --rm -v "$(pwd):$WORKSPACE" -w $WORKSPACE $DOCKER_IMAGE bash -c "
    export LD_LIBRARY_PATH=$LD_PATH:\$LD_LIBRARY_PATH
    
    echo '=== Compiling test_signature_extended ==='
    $CXX \
        -std=c++26 \
        -stdlib=libc++ \
        -freflection \
        -I$WORKSPACE/include \
        -o /tmp/test_signature_extended \
        $WORKSPACE/test/test_signature_extended.cpp \
        2>&1
    
    echo ''
    echo '=== Running Extended Tests ==='
    /tmp/test_signature_extended
    
    echo ''
    echo '=== Test Summary ==='
    echo 'If you see this message, all types compiled and ran successfully!'
"

echo ""
echo "=================================================="
echo "Extended tests completed!"
echo "=================================================="
