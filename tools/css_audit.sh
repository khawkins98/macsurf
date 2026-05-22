#!/bin/bash
# css_audit.sh - regenerate CSS support inventory for MacSurf
#
# Outputs:
#   tools/audit/parsers.txt           - libcss parse functions
#   tools/audit/computed_accessors.txt- libcss public computed_*() accessors
#   tools/audit/consumed.txt          - accessors called from netsurf/ (layout, redraw, etc.)
#   tools/audit/parsed_not_consumed.txt - accessors with no consumer
#   tools/audit/atrules.txt
#   tools/audit/selectors.txt
#   tools/audit/macsurf_props.txt     - macsurf-vendor properties
#
# Run from repo root.

set -e
cd "$(dirname "$0")/.."
mkdir -p tools/audit
OUT=tools/audit

# --- libcss parsers ---
grep -h "css__parse_" browser/libcss/src/parse/properties/*.c 2>/dev/null \
  | grep -oE "css__parse_[a-z_]+" | sort -u > "$OUT/parsers.txt"

# --- libcss public accessors ---
grep -hoE "css_computed_[a-z_]+\\s*\\(" browser/libcss/include/libcss/computed.h 2>/dev/null \
  | sed 's/(.*//' | sort -u > "$OUT/computed_accessors.txt"

# --- consumers (all of browser/netsurf) ---
grep -rhoE "css_computed_[a-z_]+\\s*\\(" browser/netsurf/ 2>/dev/null \
  | sed 's/(.*//' | sort -u > "$OUT/consumed.txt"

# --- parsed but not consumed ---
comm -23 "$OUT/computed_accessors.txt" "$OUT/consumed.txt" > "$OUT/parsed_not_consumed.txt"

# --- macsurf vendor accessors ---
grep -E "macsurf" "$OUT/computed_accessors.txt" > "$OUT/macsurf_props.txt" || true

# --- at-rules accepted by parser ---
grep -rhoE "@(media|supports|font-face|keyframes|page|import|layer|container|charset|namespace)" \
  browser/libcss/src/parse/ 2>/dev/null | sort -u > "$OUT/atrules.txt"

# --- pseudo classes/elements known to parser ---
grep -E "PSEUDO_CLASS|PSEUDO_ELEMENT" browser/libcss/src/parse/language.c 2>/dev/null \
  | grep -oE "\\{ [A-Z_]+, CSS_SELECTOR_PSEUDO_[A-Z]+ \\}" \
  | sort -u > "$OUT/selectors.txt"

# --- counts ---
{
  echo "=== CSS audit: $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
  echo "parsers:            $(wc -l < $OUT/parsers.txt)"
  echo "computed accessors: $(wc -l < $OUT/computed_accessors.txt)"
  echo "consumed:           $(wc -l < $OUT/consumed.txt)"
  echo "parsed-not-consumed:$(wc -l < $OUT/parsed_not_consumed.txt)"
  echo "macsurf vendor:     $(wc -l < $OUT/macsurf_props.txt)"
  echo "at-rules:           $(wc -l < $OUT/atrules.txt)"
  echo "pseudos:            $(wc -l < $OUT/selectors.txt)"
} | tee "$OUT/SUMMARY.txt"
