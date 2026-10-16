#!/usr/bin/env bash
set -euo pipefail

REPOSITORY_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPOSITORY_DIRECTORY"

doxygen Doxyfile
# README uses a raw HTML image with this relative path. Preserve that path in
# the generated site; HTML_EXTRA_FILES alone copies files to the HTML root.
mkdir -p build/docs/html/docs/images
cp docs/images/owplog-overview.svg build/docs/html/docs/images/
test -s build/docs/html/index.html
test -s build/docs/html/doxygen-awesome.css
echo "Documentation generated in $REPOSITORY_DIRECTORY/build/docs/html"
