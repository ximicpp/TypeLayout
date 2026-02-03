#!/bin/bash
# Build documentation locally using Antora
# Usage: ./scripts/build-docs-local.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

echo "📚 Building TypeLayout Documentation..."

# Check if Antora is installed
if ! command -v antora &> /dev/null; then
    echo "⚠️  Antora not found. Installing..."
    npm i -g @antora/cli@3.1 @antora/site-generator@3.1
fi

# Create temporary playbook for local build
PLAYBOOK=$(mktemp)
cat > "$PLAYBOOK" << 'EOF'
site:
  title: Boost.TypeLayout Documentation
  url: https://ximicpp.github.io/TypeLayout
  start_page: typelayout::index.adoc
content:
  sources:
    - url: .
      branches: HEAD
      start_path: doc
ui:
  bundle:
    url: https://gitlab.com/antora/antora-ui-default/-/jobs/artifacts/HEAD/raw/build/ui-bundle.zip?job=bundle-stable
    snapshot: true
asciidoc:
  attributes:
    experimental: true
    source-highlighter: highlight.js
EOF

cd "$ROOT_DIR"

# Build the site
antora --fetch "$PLAYBOOK"

# Cleanup
rm "$PLAYBOOK"

echo ""
echo "✅ Documentation built successfully!"
echo "📂 Output: $ROOT_DIR/build/site/"
echo ""
echo "To preview locally:"
echo "  cd build/site && python3 -m http.server 8000"
echo "  Then open: http://localhost:8000"
