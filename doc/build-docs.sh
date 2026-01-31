#!/bin/bash
# Build Boost.TypeLayout Documentation
#
# This script builds the Antora documentation site.
# Prerequisites: Node.js 18+ and npm installed
#
# Usage:
#   ./build-docs.sh          # Build documentation
#   ./build-docs.sh serve    # Build and serve locally
#   ./build-docs.sh clean    # Clean build artifacts

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
SITE_DIR="$BUILD_DIR/site"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_prerequisites() {
    log_info "Checking prerequisites..."
    
    if ! command -v node &> /dev/null; then
        log_error "Node.js is not installed. Please install Node.js 18 or later."
        exit 1
    fi
    
    NODE_VERSION=$(node -v | cut -d'v' -f2 | cut -d'.' -f1)
    if [ "$NODE_VERSION" -lt 18 ]; then
        log_warn "Node.js version $NODE_VERSION detected. Version 18+ recommended."
    fi
    
    if ! command -v npm &> /dev/null; then
        log_error "npm is not installed."
        exit 1
    fi
    
    log_info "Prerequisites OK"
}

install_antora() {
    log_info "Installing Antora..."
    
    if [ ! -d "$SCRIPT_DIR/node_modules/@antora" ]; then
        npm install --prefix "$SCRIPT_DIR" @antora/cli @antora/site-generator
    else
        log_info "Antora already installed"
    fi
}

build_docs() {
    log_info "Building documentation..."
    
    cd "$SCRIPT_DIR"
    
    # Run Antora
    npx antora --fetch antora-playbook.yml
    
    log_info "Documentation built successfully!"
    log_info "Output: $SITE_DIR"
}

serve_docs() {
    log_info "Starting local server..."
    
    if ! command -v python3 &> /dev/null; then
        log_error "Python 3 required for local server"
        exit 1
    fi
    
    cd "$SITE_DIR"
    log_info "Serving at http://localhost:8000"
    log_info "Press Ctrl+C to stop"
    
    python3 -m http.server 8000
}

clean_build() {
    log_info "Cleaning build artifacts..."
    
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        log_info "Removed $BUILD_DIR"
    fi
    
    if [ -d "$SCRIPT_DIR/node_modules" ]; then
        rm -rf "$SCRIPT_DIR/node_modules"
        log_info "Removed node_modules"
    fi
    
    if [ -f "$SCRIPT_DIR/package-lock.json" ]; then
        rm "$SCRIPT_DIR/package-lock.json"
    fi
    
    log_info "Clean complete"
}

# Main
case "${1:-build}" in
    build)
        check_prerequisites
        install_antora
        build_docs
        ;;
    serve)
        check_prerequisites
        install_antora
        build_docs
        serve_docs
        ;;
    clean)
        clean_build
        ;;
    *)
        echo "Usage: $0 {build|serve|clean}"
        exit 1
        ;;
esac
