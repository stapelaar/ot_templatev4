#!/usr/bin/env bash
set -euo pipefail

echo "🔍 Sanitizing Kconfig, .conf and .overlay files (BOM, CRLF, Unicode)…"

FILES=(Kconfig prj.conf overlays/*.conf overlays/*.overlay nodes/*/*.conf boards/*.overlay)

# Verwijder UTF-8 BOM
for f in "${FILES[@]}"; do
  [[ -f "$f" ]] || continue
  sed -i '' $'1s/^\uFEFF//' "$f"
done

# CRLF -> LF
for f in "${FILES[@]}"; do
  [[ -f "$f" ]] || continue
  awk '{ sub(/\r$/, ""); print }' "$f" > "$f.tmp" && mv "$f.tmp" "$f"
done

# Vervang veelvoorkomende “slimme” tekens door ASCII
for f in "${FILES[@]}"; do
  [[ -f "$f" ]] || continue
  # em/en dash → -
  sed -i '' 's/—/-/g; s/–/-/g' "$f"
  # slimme quotes → ' "
  sed -i '' "s/‘/'/g; s/’/'/g; s/“/\"/g; s/”/\"/g" "$f"
  # NBSP → spatie
  perl -i -pe 's/\x{A0}/ /g' "$f"
done

# Detecteer resterende non-ASCII bytes
echo "🔎 Scanning for non-ASCII…"
for f in "${FILES[@]}"; do
  [[ -f "$f" ]] || continue
  if LC_ALL=C tr -d '\0-\177' < "$f" | grep -q . ; then
    echo "⚠️  Non-ASCII in: $f"
    # Toon de probleemregels (niet-printfbare chars)
    LC_ALL=C grep -n "[^[:print:][:space:]]" "$f" || true
  fi
done

echo "✅ Sanitize-all done."
