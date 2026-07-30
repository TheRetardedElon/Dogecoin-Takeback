#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
SRC="/mnt/c/Users/Jeramiah/Downloads/worldmap-yellow.png"
DST="/mnt/c/dogedev/src/qt/res/images/worldmap-yellow.png"
mkdir -p "$(dirname "$DST")"

if command -v convert >/dev/null 2>&1; then
  convert "$SRC" -resize 1600x910 "$DST"
elif command -v magick >/dev/null 2>&1; then
  magick "$SRC" -resize 1600x910 "$DST"
elif command -v ffmpeg >/dev/null 2>&1; then
  ffmpeg -y -i "$SRC" -vf scale=1600:-1 "$DST"
else
  # try install imagemagick
  sudo apt-get update -qq
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq imagemagick
  convert "$SRC" -resize 1600x910 "$DST"
fi
file "$DST"
ls -la "$DST"
