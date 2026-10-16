#!/usr/bin/env bash
set -euo pipefail

REPOSITORY_DIRECTORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPOSITORY_DIRECTORY"

# Doxygen opens WARN_LOGFILE before creating its output tree on a fresh checkout.
mkdir -p build/docs
doxygen Doxyfile
# README uses a raw HTML image with this relative path. Preserve that path in
# the generated site; HTML_EXTRA_FILES alone copies files to the HTML root.
mkdir -p build/docs/html/docs/images
cp docs/images/owplog-overview.svg build/docs/html/docs/images/
# Preserve Doxygen's underscore-prefixed assets when hosting with GitHub Pages.
touch build/docs/html/.nojekyll
test -s build/docs/html/index.html
test -s build/docs/html/doxygen-awesome.css
echo "Documentation generated in $REPOSITORY_DIRECTORY/build/docs/html"
