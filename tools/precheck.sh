#!/bin/bash
# precheck.sh — host-side native C89 syntax gate for MacSurf
#
# Runs the host C compiler (Apple clang / gcc) in -std=c89 -pedantic-errors mode
# over every compilable source file listed in the MacSurf.mcp manifest, in
# parallel across all host cores. Catches the dominant failure class (C89 /
# include-path errors) in seconds, BEFORE spending a multi-minute emulated
# CodeWarrior build cycle discovering the same error 256 files in.
#
#   ./tools/precheck.sh             # check everything checkable on this host
#   ./tools/precheck.sh -v          # also list per-file PASS lines
#   ./tools/precheck.sh <file.c>... # check specific file(s) only
#
# Exit status: 0 = all checked files pass, 1 = at least one failure.
#
# What this is NOT:
#  - Not a substitute for the real CW8 build. clang-clean != CW8-clean (CW8 has
#    its own miscompiles, MSL headers, and stricter pointer-type rules; see
#    CLAUDE.md "Known Gotchas"). This gate only eliminates the cheap failures.
#  - Frontend files that include Mac Toolbox headers (<Carbon.h>, <MacTypes.h>)
#    need Apple Universal Interfaces headers. Set UNIV_HDRS to a CIncludes
#    directory to enable them (extract from the CW8 install in the QEMU disk
#    image, or from an Apple Universal Interfaces 3.4.x distribution).
#    Without UNIV_HDRS those files are SKIPPED, not failed.
#
# Companion to (not a replacement for) the maintainer's Linux-side scripts:
#   scripts/verify_macsurf.sh (powerpc-linux-gnu-gcc + Universal Interfaces)
#   scripts/test-build.sh     (Retro68 powerpc-apple-macos-gcc)

set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
MCP="$REPO/browser/netsurf/frontends/macos9/MacSurf.mcp"
CC="${CC:-clang}"
UNIV_HDRS="${UNIV_HDRS:-$HOME/universal_headers}"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
VERBOSE=0

# ---------------------------------------------------------------------------
# Compiler flags — approximate CW8's C89 mode as closely as a host compiler can
# ---------------------------------------------------------------------------
PREFIX_H="$REPO/browser/netsurf/frontends/macos9/macsurf_prefix.h"

FLAGS=(
  -fsyntax-only
  -std=c89
  # The real prefix file. It is host-portable by design (#ifndef __MWERKS__
  # branches exist for Linux/macOS syntax-checking) and provides the same
  # environment CW8 injects: NSLOG no-op, inline/restrict erasure, __MACOS9__,
  # struct tm locking, string.h forward decls, FLEX_ARRAY_LEN_DECL, etc.
  -include "$PREFIX_H"
  # -pedantic emits informational warnings for everything non-C89; only the
  # explicit -Werror= list below GATES the result. This is deliberate: bare
  # -Wpedantic diagnostics include things CW8 accepts (e.g. void* <-> function
  # pointer conversion in box_construct.c) that have no individual -Wno- flag.
  -pedantic
  #
  # FLAG ORDER MATTERS: broad -Wno- group disables FIRST, then specific
  # -Werror= re-enables. (-Wc99-designator and -Wgcc-compat are subgroups of
  # -Wc99-extensions; a later -Wno-c99-extensions silently disables them.)
  #
  # Noise suppression — constructs CW8 accepts (each suppression documents WHY;
  # do not add new ones without confirming CW8 accepts the construct):
  -Wno-c99-extensions    # trailing enum commas (196 in tree, CW8 accepts); costs
                         # flexible-array + compound-literal detection (both rare,
                         # both on the manual audit checklist)
  -Wno-extra-semi        # stray ';' after function bodies — CW8 accepts
  -Wno-variadic-macros   # CW8 accepts __VA_ARGS__ as a pre-C99 extension (CLAUDE.md)
  -Wno-long-long         # CW8 accepts long long; the multiply MISCOMPILE is a runtime
                         # hazard tracked in CLAUDE.md gotchas, not a compile error
  -Wno-implicit-function-declaration  # legal in C89 (clang 16+ errors by default);
                                      # CW8 warns but compiles
  -Wno-gnu-statement-expression-from-macro-expansion  # talloc.h gates these behind
                                      # __GNUC__; CW8 takes the non-GNU branch
  -Wno-newline-eof       # missing newline at EOF — cosmetic
  -Wno-format-pedantic   # printf %p pedantry — CW8/MSL doesn't check formats
  -Wno-overlength-strings # C90's 509-char string literal limit — CW8 accepts longer
  #
  # GATING ERRORS — the documented CW8 build breakers (CLAUDE.md audit checklist).
  # Must come AFTER the -Wno- group disables above:
  -Werror=c99-designator              # designated initializers (.field and [idx] forms)
  -Werror=declaration-after-statement # mid-block declarations
  -Werror=gcc-compat                  # for-scope declarations (`for (int i...`)
  -Werror=comment                     # // line comments
  -Werror=vla-extension               # variable-length arrays
)

# Include paths mirroring the CW8 access paths (order matters: macos9 shims
# shadow real headers, exactly as the access-path order does in the IDE).
INCLUDES=(
  -I"$REPO/browser/netsurf/frontends/macos9"
  -I"$REPO/browser/netsurf/frontends/macos9/shims"
  -I"$REPO/browser/netsurf/frontends/macos9/javascript"
  -I"$REPO/browser/netsurf/include"
  -I"$REPO/browser/netsurf/utils"
  -I"$REPO/browser/netsurf/content"
  -I"$REPO/browser/netsurf/content/handlers"
  -I"$REPO/browser/netsurf/desktop"
  -I"$REPO/browser/netsurf/frontends"
  -I"$REPO/browser/netsurf"
  -I"$REPO/browser/libdom/include"
  -I"$REPO/browser/libdom/src"
  -I"$REPO/browser/libdom/src/core"
  -I"$REPO/browser/libdom/src/events"
  -I"$REPO/browser/libdom/src/html"
  -I"$REPO/browser/libdom/src/utils"
  -I"$REPO/browser/libdom/bindings/hubbub"
  -I"$REPO/browser/libcss/include"
  -I"$REPO/browser/libcss/src"
  -I"$REPO/browser/libhubbub/include"
  -I"$REPO/browser/libhubbub/src"
  -I"$REPO/browser/libhubbub/src/utils"
  -I"$REPO/browser/libparserutils/include"
  -I"$REPO/browser/libparserutils/src"
  -I"$REPO/browser/libwapcaplet/include"
  -I"$REPO/browser/libduktape"
)

# ---------------------------------------------------------------------------
# Skip lists
# ---------------------------------------------------------------------------
# Files that cannot be checked on a macOS host: the macos9 POSIX shims they
# pull in (sys/time.h, mac_inet.h, struct tm) collide with the macOS SDK
# headers, or they include Mac-side-only generated headers. They remain
# checkable on the maintainer's Linux setup (scripts/verify_macsurf.sh) and in
# the real CW8 build. Exact file list (audited 2026-06-01); if a new file
# starts colliding, add it here with the reason rather than a directory glob —
# directory globs hid 40 perfectly checkable files (layout.c, redraw.c, ...).
HOST_COLLISION_FILES=(
  # -- shims sys/time.h vs SDK struct timeval / htons / struct tm --
  "browser/libduktape/duktape.c"
  "browser/netsurf/content/content.c"
  "browser/netsurf/content/fetch.c"
  "browser/netsurf/content/hlcache.c"
  "browser/netsurf/content/llcache.c"
  "browser/netsurf/content/no_backing_store.c"
  "browser/netsurf/content/textsearch.c"
  "browser/netsurf/content/urldb.c"
  "browser/netsurf/content/handlers/css/cssh_css.c"
  "browser/netsurf/content/handlers/html/box_special.c"
  "browser/netsurf/content/handlers/html/css_fetcher.c"
  "browser/netsurf/content/handlers/html/form.c"
  "browser/netsurf/content/handlers/html/imagemap.c"
  "browser/netsurf/content/handlers/html/script.c"
  "browser/netsurf/desktop/browser_window.c"
  "browser/netsurf/desktop/gui_factory.c"
  "browser/netsurf/desktop/netsurf.c"
  "browser/netsurf/desktop/selection.c"
  "browser/netsurf/utils/filepath.c"
  "browser/netsurf/utils/idna.c"
  "browser/netsurf/utils/log.c"
  "browser/netsurf/utils/nsoption.c"
  "browser/netsurf/utils/utils.c"
  "browser/libdom/bindings/hubbub/parser.c"
  "browser/libdom/src/utils/character_valid.c"
  "browser/libdom/src/utils/validate.c"
  "browser/libhubbub/src/parser.c"
  "browser/libhubbub/src/treebuilder/in_head.c"
  # -- Mac-side-only / generated headers --
  "browser/libparserutils/src/charset/aliases.c"
  "browser/libcss/src/stylesheet.c"
  "browser/libcss/src/parse/font_face.c"
  "browser/libcss/src/parse/p_font_face.c"
  "browser/libcss/src/parse/properties/css_property_parser_gen.c"
  # -- prefix's isascii macro / struct tm vs SDK <ctype.h> / <time.h> --
  "browser/libdom/src/events/event.c"
  "browser/netsurf/content/handlers/css/cssh_select.c"
  "browser/netsurf/content/handlers/html/box_textarea.c"
  "browser/netsurf/content/handlers/html/font.c"
  "browser/netsurf/content/handlers/html/html.c"
  "browser/netsurf/content/handlers/html/interaction.c"
  "browser/netsurf/content/handlers/html/object.c"
  "browser/netsurf/content/handlers/html/redraw_border.c"
  "browser/netsurf/content/handlers/html/textselection.c"
  "browser/netsurf/utils/nscolour.c"
  "browser/netsurf/utils/url.c"
)

usage() { sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0; }

# ---------------------------------------------------------------------------
# Build the file list from the manifest
# ---------------------------------------------------------------------------
resolve_path() {
  # Manifest paths are relative to browser/netsurf/frontends/macos9/
  local p="$1"
  case "$p" in
    ../../../../*) echo "browser/${p#../../../../}" ;;
    ../../../*)    echo "browser/netsurf/${p#../../../}" ;;
    *)             echo "browser/netsurf/frontends/macos9/$p" ;;
  esac
}

needs_toolbox() {
  # Frontend + shim files include Mac Toolbox headers via macos9.h /
  # macsurf_prefix.h and need Universal Interfaces to check.
  case "$1" in
    browser/netsurf/frontends/macos9/*) return 0 ;;
    *) return 1 ;;
  esac
}

is_host_collision() {
  local f="$1" k
  for k in "${HOST_COLLISION_FILES[@]}"; do
    [ "$f" = "$k" ] && return 0
  done
  return 1
}

# ---------------------------------------------------------------------------
# Per-file check (invoked in parallel via xargs)
# ---------------------------------------------------------------------------
if [ "${1:-}" = "--check-one" ]; then
  shift
  f="$1"
  errfile="$2"
  # Reconstruct arrays (exported as strings)
  # shellcheck disable=SC2086
  if $CC $PRECHECK_FLAGS $PRECHECK_INCLUDES "$REPO/$f" >"$errfile" 2>&1; then
    echo "PASS $f"
  else
    echo "FAIL $f"
  fi
  exit 0
fi

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
explicit_files=()
while [ $# -gt 0 ]; do
  case "$1" in
    -h|--help) usage ;;
    -v) VERBOSE=1 ;;
    *)  explicit_files+=("$1") ;;
  esac
  shift
done

if [ ! -f "$MCP" ]; then
  echo "error: manifest not found: $MCP" >&2
  exit 1
fi

# Universal Interfaces available?
TOOLBOX_OK=0
if [ -d "$UNIV_HDRS" ] && [ -f "$UNIV_HDRS/MacTypes.h" ]; then
  TOOLBOX_OK=1
  INCLUDES+=(-isystem "$UNIV_HDRS")
  FLAGS+=(-D__MWERKS__=0x8000 -DTARGET_API_MAC_CARBON=1 "-D__option(x)=0" "-D__dest_os=8")
fi

# Collect candidate files
candidates=()
if [ ${#explicit_files[@]} -gt 0 ]; then
  for f in "${explicit_files[@]}"; do
    candidates+=("${f#"$REPO"/}")
  done
else
  while IFS= read -r p; do
    case "$p" in *.c) ;; *) continue ;; esac # sources only (skip libs)
    candidates+=("$(resolve_path "$p")")
  done < <(grep '<PATH>' "$MCP" | sed 's/.*<PATH>//;s/<\/PATH>.*//' | sort -u)
fi

# Partition
to_check=()
skipped_toolbox=()
skipped_collision=()
missing=()
for f in "${candidates[@]}"; do
  if [ ! -f "$REPO/$f" ]; then
    missing+=("$f"); continue
  fi
  if needs_toolbox "$f" && [ "$TOOLBOX_OK" -eq 0 ]; then
    skipped_toolbox+=("$f"); continue
  fi
  if is_host_collision "$f"; then
    skipped_collision+=("$f"); continue
  fi
  to_check+=("$f")
done

echo "MacSurf precheck — host C89 gate"
echo "  compiler:   $($CC --version | head -1)"
echo "  manifest:   ${MCP#"$REPO"/} (${#candidates[@]} source files)"
echo "  checking:   ${#to_check[@]}  |  skipped (need Universal Interfaces): ${#skipped_toolbox[@]}  |  skipped (host-SDK collision): ${#skipped_collision[@]}  |  missing: ${#missing[@]}"
[ "$TOOLBOX_OK" -eq 1 ] && echo "  toolbox:    Universal Interfaces found at $UNIV_HDRS — frontend files included"
echo

if [ ${#missing[@]} -gt 0 ]; then
  echo "WARNING: manifest lists files that do not exist in the repo:"
  printf '  %s\n' "${missing[@]}"
  echo
fi

# Export flags/includes as flat strings for the parallel workers
export PRECHECK_FLAGS="${FLAGS[*]}"
export PRECHECK_INCLUDES="${INCLUDES[*]}"
export REPO CC

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

# Run checks in parallel
i=0
for f in "${to_check[@]}"; do
  printf '%s\t%s/%d.err\n' "$f" "$tmpdir" "$i"
  i=$((i+1))
done | xargs -P "$JOBS" -L1 bash -c '"'"$0"'" --check-one "$1" "$2"' _ > "$tmpdir/results.txt"

pass=$(grep -c '^PASS ' "$tmpdir/results.txt" || true)
fail=$(grep -c '^FAIL ' "$tmpdir/results.txt" || true)

if [ "$VERBOSE" -eq 1 ]; then
  grep '^PASS ' "$tmpdir/results.txt" | sort
fi

if [ "$fail" -gt 0 ]; then
  echo "FAILURES ($fail):"
  echo
  # Print each failed file with the first lines of its error output
  grep '^FAIL ' "$tmpdir/results.txt" | sort | while read -r _ f; do
    echo "--- $f"
    # find its errfile by re-deriving index is fragile; grep all errfiles instead
    grep -l "$f" "$tmpdir"/*.err 2>/dev/null | head -1 | xargs -I{} head -12 {} | sed 's/^/    /'
    echo
  done
fi

echo "================================================================"
echo "precheck: $pass passed, $fail failed, $(( ${#skipped_toolbox[@]} + ${#skipped_collision[@]} )) skipped"
[ ${#skipped_toolbox[@]} -gt 0 ] && [ "$TOOLBOX_OK" -eq 0 ] && \
  echo "hint: set UNIV_HDRS=<path to Universal Interfaces CIncludes> to also check the ${#skipped_toolbox[@]} frontend files"

[ "$fail" -eq 0 ]
