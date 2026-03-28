#!/bin/bash
# Convert a BMP/PNG image to a GxEPD2-compatible C header (1-bit, inverted format).
# Uses ImageMagick (magick) and xxd.
#
# Usage: ./tools/bmp2header.sh <input_image> <output.h> <array_name> [width height]
#
# The image is converted to 1-bit monochrome via PBM, then inverted for GxEPD2's
# drawInvertedBitmap() format (0xFF=white, 0x00=black, MSB first).
#
# If width/height are omitted, the image's native dimensions are used.
#
# Examples:
#   ./tools/bmp2header.sh nsec_200x200_bw_ordered.bmp lib/core/hardware/nsec_logo.h LOGO_BITMAP
#   ./tools/bmp2header.sh nsec_200xH_bw_ordered.bmp lib/core/hardware/nsec_logo_half.h LOGO_HALF_BITMAP 200 105

set -euo pipefail

if [ $# -lt 3 ]; then
  echo "Usage: $0 <input_image> <output.h> <array_name> [width height]"
  echo ""
  echo "Converts an image to a GxEPD2 1-bit PROGMEM C header."
  echo "Use with drawInvertedBitmap()."
  exit 1
fi

INPUT="$1"
OUTPUT="$2"
NAME="$3"

if [ ! -f "$INPUT" ]; then
  echo "Error: $INPUT not found"
  exit 1
fi

command -v magick >/dev/null 2>&1 || { echo "Error: ImageMagick (magick) not found"; exit 1; }

# Get dimensions
if [ $# -ge 5 ]; then
  WIDTH="$4"
  HEIGHT="$5"
else
  DIMS=$(magick identify -format "%w %h" "$INPUT")
  WIDTH=$(echo "$DIMS" | cut -d' ' -f1)
  HEIGHT=$(echo "$DIMS" | cut -d' ' -f2)
fi

EXPECTED_BYTES=$(( WIDTH * HEIGHT / 8 ))

echo "Converting: $INPUT -> $OUTPUT"
echo "  Dimensions: ${WIDTH}x${HEIGHT}"
echo "  Array name: badge::${NAME}"
echo "  Expected size: ${EXPECTED_BYTES} bytes"

# Convert to PBM (1-bit, well-defined top-to-bottom MSB-first format)
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

magick "$INPUT" "pbm:${TMPDIR}/img.pbm"

# Extract raw data (skip PBM header), invert for GxEPD2 format
python3 -c "
d = open('${TMPDIR}/img.pbm', 'rb').read()
i = d.index(b'\n', 3) + 1
raw = d[i:]
if len(raw) != ${EXPECTED_BYTES}:
    raise ValueError(f'Expected ${EXPECTED_BYTES} bytes, got {len(raw)}')
inverted = bytes(b ^ 0xFF for b in raw)
open('${TMPDIR}/img.bin', 'wb').write(inverted)
print(f'  Converted: {len(inverted)} bytes OK')
"

# Generate C header
cat > "$OUTPUT" << HEADER
#pragma once

#include <Arduino.h>

// ${WIDTH}x${HEIGHT} 1-bit bitmap (GxEPD2 inverted format: 0xFF=white, 0x00=black)
// Use with drawInvertedBitmap(). Auto-generated from $(basename "$INPUT")

namespace badge {

static constexpr uint16_t ${NAME}_WIDTH = ${WIDTH};
static constexpr uint16_t ${NAME}_HEIGHT = ${HEIGHT};

// clang-format off
static const uint8_t ${NAME}[] PROGMEM = {
HEADER

xxd -i < "${TMPDIR}/img.bin" >> "$OUTPUT"

cat >> "$OUTPUT" << FOOTER

};
// clang-format on

}  // namespace badge
FOOTER

# Verify byte count
ACTUAL=$(python3 -c "
import re
with open('${OUTPUT}') as f:
    content = f.read()
start = content.index('PROGMEM = {') + len('PROGMEM = {')
end = content.index('};', start)
print(len(re.findall(r'0x[0-9a-fA-F]+', content[start:end])))
")

if [ "$ACTUAL" != "$EXPECTED_BYTES" ]; then
  echo "ERROR: Expected ${EXPECTED_BYTES} bytes in array, got ${ACTUAL}"
  exit 1
fi

echo "  Output: $OUTPUT (${ACTUAL} bytes in array)"
echo "Done."
