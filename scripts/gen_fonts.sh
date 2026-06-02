#!/usr/bin/env bash
# Convert HelveticaNeue.ttc into the Adafruit-GFX bitmap font headers the firmware
# needs: src/fonts/HelveticaNeue{Regular8,Medium10,Bold26}pt7b.h
#
# The .ttc (proprietary) and the generated headers are gitignored, so run this once
# after cloning, before building.
#
# Requirements:
#   - HelveticaNeue.ttc in the repo root (macOS: cp /System/Library/Fonts/HelveticaNeue.ttc .)
#   - freetype:   brew install freetype
#   - fonttools:  pip3 install fonttools
#   - Adafruit GFX library fetched into .pio (run `pio run` once first)
set -euo pipefail
cd "$(dirname "$0")/.."

TTC="HelveticaNeue.ttc"
OUT="src/fonts"
WORK=".pio/fonttool"
CC="${CC:-/usr/bin/cc}"   # the shell's `cc` is sometimes shadowed; use the real compiler

[ -f "$TTC" ] || { echo "ERROR: $TTC not found in repo root."; \
    echo "  macOS: cp /System/Library/Fonts/HelveticaNeue.ttc ."; exit 1; }

FT="$(brew --prefix freetype 2>/dev/null || true)"
[ -n "$FT" ] && [ -d "$FT" ] || { echo "ERROR: freetype not found. Install: brew install freetype"; exit 1; }

FC_SRC="$(find .pio/libdeps -path '*Adafruit GFX Library/fontconvert/fontconvert.c' 2>/dev/null | head -1)"
[ -n "$FC_SRC" ] || { echo "ERROR: Adafruit GFX fontconvert.c not found."; \
    echo "  Run 'pio run' once to fetch libraries, then retry."; exit 1; }

python3 -c "import fontTools" 2>/dev/null || { echo "ERROR: fonttools missing. Install: pip3 install fonttools"; exit 1; }

mkdir -p "$WORK" "$OUT"

echo "Building fontconvert..."
"$CC" "$FC_SRC" -I"$FT/include/freetype2" -L"$FT/lib" -lfreetype -o "$WORK/fontconvert"

echo "Extracting faces from $TTC..."
# fontconvert only reads face 0 of a file, so split the needed weights out first.
python3 - "$TTC" "$WORK" <<'PY'
import sys
from fontTools.ttLib import TTCollection
ttc, work = sys.argv[1], sys.argv[2]
c = TTCollection(ttc)
for idx, name in {0: "HelveticaNeueRegular", 10: "HelveticaNeueMedium", 1: "HelveticaNeueBold"}.items():
    c.fonts[idx].save(f"{work}/{name}.ttf")
PY

FCV="$WORK/fontconvert"
echo "Generating GFX headers into $OUT/ ..."
# fontconvert renders at 141 DPI, so pixel-em ~= point-size * 1.96.
# Regular 8pt, full ASCII (0x20-0x7E) — small body text / hints
"$FCV" "$WORK/HelveticaNeueRegular.ttf" 8        > "$OUT/HelveticaNeueRegular8pt7b.h"
# Regular 9pt, full ASCII — weather right-column items (low/high, precip, description)
"$FCV" "$WORK/HelveticaNeueRegular.ttf" 9         > "$OUT/HelveticaNeueRegular9pt7b.h"
# Medium 10pt, full ASCII — headers / conditions
"$FCV" "$WORK/HelveticaNeueMedium.ttf"  10       > "$OUT/HelveticaNeueMedium10pt7b.h"
# Bold 30pt, '*'(0x2A)..':'(0x3A) only — factory-reset countdown digit
"$FCV" "$WORK/HelveticaNeueBold.ttf"    30 42 58 > "$OUT/HelveticaNeueBold30pt7b.h"
# Bold 57pt, '*'..':' only — full-height, left-justified temperature on the weather screen
"$FCV" "$WORK/HelveticaNeueBold.ttf"    57 42 58 > "$OUT/HelveticaNeueBold57pt7b.h"

echo "Done:"
ls -l "$OUT"/HelveticaNeue*pt7b.h