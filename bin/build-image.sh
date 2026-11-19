#!/usr/bin/env bash
# Build a complete, flashable Meshtastic image set for one ESP32-S3 board, including the
# OTA loader built from the local esp32-unified-ota checkout.
#
#   bin/build-image.sh heltec-v3-handheld
#
# Expected layout (override with the environment variables in brackets):
#   <root>/firmware/            this repository
#   <root>/esp32-unified-ota/   OTA loader checkout            [OTA_DIR]
#   <root>/env.sh               optional toolchain environment, sourced if present
#   <root>/out/<env>/           output                         [OUT_DIR]
#
# Output files:
#   firmware-<env>-<ver>.factory.bin   bootloader + partition table + app (flash at 0x0)
#   firmware-<env>-<ver>.bin           app only (this is the file to send for OTA updates)
#   littlefs-<env>-<ver>.bin           filesystem
#   mt-esp32s3-ota.bin                 OTA loader (goes in the second app partition)
#   firmware-<env>-<ver>.full.bin      all of the above merged; flash at 0x0 after an erase
#   device-install.sh / device-update.sh   upstream flashing scripts
set -euo pipefail

ENV_NAME="${1:?usage: $0 <platformio-env>   e.g. heltec-v3-handheld, heltec-ws-v3, heltec-v3}"
FW="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROOT="$(dirname "$FW")"
if [[ -f "$ROOT/env.sh" ]]; then
    # shellcheck disable=SC1091
    source "$ROOT/env.sh"
fi
OTA="${OTA_DIR:-$ROOT/esp32-unified-ota}"
if [[ ! -d "$OTA" ]]; then
    echo "error: OTA loader checkout not found at $OTA (set OTA_DIR)" >&2
    exit 1
fi
OTA_ENV=esp32s3
OTA_NAME="mt-$OTA_ENV-ota.bin"

echo "== OTA loader ($OTA_ENV)"
(cd "$OTA" && git submodule update --init --recursive -q && pio run -e "$OTA_ENV")

echo "== firmware ($ENV_NAME)"
cd "$FW"
VERSION="$(bin/buildinfo.py long)"
export APP_VERSION="$VERSION"
pio run -e "$ENV_NAME" -t mtjson

BUILD="$FW/.pio/build/$ENV_NAME"
BASE="firmware-$ENV_NAME-$VERSION"
LFS="littlefs-$ENV_NAME-$VERSION.bin"
META="$BUILD/$BASE.mt.json"

MCU="$(jq -r '.mcu' "$META")"
if [[ "$MCU" != "$OTA_ENV" ]]; then
    echo "error: $ENV_NAME is a $MCU board; this script only handles $OTA_ENV" >&2
    exit 1
fi
OTA_OFFSET="$(jq -r '.part[] | select(.subtype == "ota_1") | .offset' "$META")"
OTA_SLOT="$(jq -r '.part[] | select(.subtype == "ota_1") | .size' "$META")"
LFS_OFFSET="$(jq -r '.part[] | select(.subtype == "spiffs") | .offset' "$META")"
OTA_BYTES="$(stat -c %s "$OTA/.pio/build/$OTA_ENV/firmware.bin")"
if (( OTA_BYTES > OTA_SLOT )); then
    echo "error: OTA loader ($OTA_BYTES bytes) does not fit the ota_1 partition ($OTA_SLOT)" >&2
    exit 1
fi

OUT="${OUT_DIR:-$ROOT/out}/$ENV_NAME"
rm -rf "$OUT"
mkdir -p "$OUT"
cp "$BUILD/$BASE.factory.bin" "$BUILD/$BASE.bin" "$BUILD/$LFS" "$META" "$OUT/"
cp "$OTA/.pio/build/$OTA_ENV/firmware.bin" "$OUT/$OTA_NAME"
cp "$FW/bin/device-install.sh" "$FW/bin/device-update.sh" "$OUT/"

# Record the OTA loader in the manifest the same way the upstream release pipeline does.
jq --arg name "$OTA_NAME" --arg md5 "$(md5sum "$OUT/$OTA_NAME" | cut -d' ' -f1)" \
   --argjson bytes "$OTA_BYTES" \
   '.files += [{name: $name, md5: $md5, bytes: $bytes, part_name: "app1"}]' \
   "$META" > "$OUT/$BASE.mt.json"

echo "== merged image (OTA loader at $OTA_OFFSET, filesystem at $LFS_OFFSET)"
python -m esptool --chip "$MCU" merge-bin -o "$OUT/$BASE.full.bin" \
    --flash-mode dio --flash-freq 80m --flash-size 8MB \
    0x0 "$OUT/$BASE.factory.bin" \
    "$OTA_OFFSET" "$OUT/$OTA_NAME" \
    "$LFS_OFFSET" "$OUT/$LFS"

echo
ls -l "$OUT"
echo
echo "Flash everything (erases the device, including its settings):"
echo "  python -m esptool --chip $MCU --port /dev/ttyUSB0 --baud 921600 erase-flash"
echo "  python -m esptool --chip $MCU --port /dev/ttyUSB0 --baud 921600 write-flash 0x0 $OUT/$BASE.full.bin"
