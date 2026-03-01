#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${1:-.}"
MAX_ENTRIES=10
MAX_LOC=250

cd "$ROOT_DIR"

echo "=== Directory entry count audit (max ${MAX_ENTRIES}) ==="
dir_violations=0
while IFS= read -r -d '' dir; do
  rel="${dir#./}"
  [[ "$rel" == "." ]] && rel=""

  if [[ "$rel" == build* ]] || [[ "$rel" == .git* ]] || [[ "$rel" == .vs* ]] || [[ "$rel" == .vscode* ]] || [[ "$rel" == node_modules* ]] || [[ "$rel" == .venv* ]]; then
    continue
  fi

  count=$(find "$dir" -mindepth 1 -maxdepth 1 | wc -l | tr -d ' ')
  if [[ "$count" -gt "$MAX_ENTRIES" ]]; then
    echo "DIR_VIOLATION $rel -> $count entries"
    dir_violations=$((dir_violations + 1))
  fi
done < <(find . -type d -print0)

echo ""
echo "=== File LOC audit (max ${MAX_LOC}) ==="
file_violations=0
while IFS= read -r -d '' file; do
  rel="${file#./}"

  if [[ "$rel" == build/* ]] || [[ "$rel" == .git/* ]] || [[ "$rel" == node_modules/* ]] || [[ "$rel" == .venv/* ]]; then
    continue
  fi

  loc=$(wc -l < "$file" | tr -d ' ')
  if [[ "$loc" -gt "$MAX_LOC" ]]; then
    echo "FILE_VIOLATION $rel -> ${loc} LOC"
    file_violations=$((file_violations + 1))
  fi
done < <(find . -type f \( -name "*.h" -o -name "*.hpp" -o -name "*.cpp" -o -name "*.c" -o -name "*.mm" -o -name "*.md" \) -print0)

echo ""
echo "Summary: ${dir_violations} directory violations, ${file_violations} file violations"

if [[ "$dir_violations" -gt 0 || "$file_violations" -gt 0 ]]; then
  exit 2
fi
