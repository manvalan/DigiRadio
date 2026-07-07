#!/usr/bin/env bash
# Regenerate the gzipped setup UI embedded by components/net/CMakeLists.txt.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${ROOT}/components/net/www/index.html"
OUT="${ROOT}/components/net/www/index.html.gz"
gzip -c -9 "$SRC" > "$OUT"
echo "Wrote $(wc -c < "$OUT" | tr -d ' ') bytes -> ${OUT#"$ROOT"/}"
