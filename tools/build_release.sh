#!/bin/sh
# Builds the release firmware and the web installer site into .pio/release/
# (c) 2025 Honza Skýpala
# WTFPL license applies
#
# Usage: tools/build_release.sh <version>     e.g. tools/build_release.sh 1.0.0
# Output: .pio/release/bcp-clock-<version>.bin (merged image, flashed at offset 0; erases the settings)
#         .pio/release/site/ (install page, ESP Web Tools manifest and the image parts, for GitHub Pages)

set -e

VERSION="$1"
if [ -z "$VERSION" ]; then
    echo "Usage: $0 <version>" >&2
    exit 1
fi

cd "$(dirname "$0")/.."

PIO="${PIO:-pio}"
command -v "$PIO" >/dev/null 2>&1 || PIO="$HOME/.platformio/penv/bin/pio"

ENV=tc001     # release build configuration of the TC001 (configs/tc001.ini)
BUILD=.pio/build/$ENV
OUT=.pio/release
SITE="$OUT/site"
BIN="bcp-clock-$VERSION.bin"
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

PLATFORMIO_BUILD_FLAGS="-DBCP_VERSION=\\\"$VERSION\\\"" "$PIO" run -e "$ENV"

rm -rf "$OUT"
mkdir -p "$SITE"

# ---- Merged image: bootloader, partition table, OTA data and application ----
"$PIO" pkg exec -p tool-esptoolpy -- esptool.py --chip esp32 merge_bin -o "$OUT/$BIN" \
    0x1000 "$BUILD/bootloader.bin" \
    0x8000 "$BUILD/partitions.bin" \
    0xe000 "$BOOT_APP0" \
    0x10000 "$BUILD/firmware.bin"

# ---- Web installer site ----
# Separate parts instead of the merged image: the merged image also overwrites NVS (0x9000) with 0xFF,
# which would delete the stored WiFi networks and settings on every update.
cp "$BUILD/bootloader.bin" "$BUILD/partitions.bin" "$BOOT_APP0" "$SITE/"
cp "$BUILD/firmware.bin" "$SITE/firmware-$VERSION.bin"
cp install/index.html "$SITE/"
cat > "$SITE/manifest.json" <<EOF
{
    "name": "BCP Clock",
    "version": "$VERSION",
    "new_install_prompt_erase": true,
    "builds": [
        {
            "chipFamily": "ESP32",
            "parts": [
                { "path": "bootloader.bin", "offset": 4096 },
                { "path": "partitions.bin", "offset": 32768 },
                { "path": "boot_app0.bin", "offset": 57344 },
                { "path": "firmware-$VERSION.bin", "offset": 65536 }
            ]
        }
    ]
}
EOF

echo "Release $VERSION built in $OUT"
