#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/bin:/bin:/usr/local/bin
B=/home/theretardedelon/dogedev-winbuild
SRC=/mnt/c/dogedev
OUT=/mnt/c/dogedev/release
VERSION=1.14.102
REL=dogecoin-${VERSION}-win64

cd "${B}/src"
cp -f "${SRC}/src/qt/win_image_decode.cpp" "${SRC}/src/qt/win_image_decode.h" qt/

# shellcheck disable=SC1090
# Export QT_INCLUDES from Makefile
QT_INCLUDES=$(sed -n 's/^QT_INCLUDES = //p' Makefile | head -1)
echo "QT_INCLUDES=${QT_INCLUDES}"

rm -f qt/libdogecoinqt_a-win_image_decode.o
# shellcheck disable=SC2086
x86_64-w64-mingw32-g++ -std=c++11 -DHAVE_CONFIG_H \
  -I. -I../src/config -I. -I./obj -I./qt -I./qt/forms \
  -mthreads \
  ${QT_INCLUDES} \
  -I./leveldb/include -I./leveldb/helpers/memenv \
  -I./secp256k1/include -I./univalue/include \
  -DHAVE_BUILD_INFO -D__STDC_FORMAT_MACROS -D_MT -DWIN32 -D_WINDOWS \
  -DBOOST_THREAD_USE_LIB -pipe -O2 \
  -fno-strict-aliasing \
  -c -o qt/libdogecoinqt_a-win_image_decode.o qt/win_image_decode.cpp

ls -la qt/libdogecoinqt_a-win_image_decode.o
sz=$(stat -c%s qt/libdogecoinqt_a-win_image_decode.o)
[[ "$sz" -gt 500 ]] || { echo "FATAL tiny win_image_decode.o"; exit 1; }

ar r qt/libdogecoinqt.a qt/libdogecoinqt_a-win_image_decode.o
ranlib qt/libdogecoinqt.a
nm qt/libdogecoinqt.a | grep DecodeImageBytesWin | head -5

rm -f qt/dogecoin-qt.exe
make qt/dogecoin-qt.exe
test -f qt/dogecoin-qt.exe
ls -lh qt/dogecoin-qt.exe dogecoind.exe dogecoin-cli.exe

# Ensure dogecoind still has dump symbols
x86_64-w64-mingw32-strings dogecoind.exe | grep -E 'dumptxoutset|fetchassumeutxomanifest' | head -5

mkdir -p "${B}/release" "${OUT}"
cp -f dogecoind.exe dogecoin-cli.exe "${B}/release/"
[[ -f dogecoin-tx.exe ]] && cp -f dogecoin-tx.exe "${B}/release/" || true
cp -f qt/dogecoin-qt.exe "${B}/release/"
x86_64-w64-mingw32-strip -s "${B}/release"/*.exe || true
date -u +"FULL_RELEASE %Y-%m-%dT%H:%M:%SZ" > "${B}/release/BUILD_STAMP.txt"
ls -la "${B}/release"/*.exe

STAGE="${B}/release-staging/${REL}"
rm -rf "${STAGE}"
mkdir -p "${STAGE}/bin" "${STAGE}/daemon"
cp -f "${B}/release/dogecoin-qt.exe" "${STAGE}/"
cp -f "${B}/release/dogecoind.exe" "${STAGE}/daemon/"
cp -f "${B}/release/dogecoin-cli.exe" "${STAGE}/daemon/"
cp -f "${B}/release/dogecoin-cli.exe" "${STAGE}/bin/"
[[ -f ${B}/release/dogecoin-tx.exe ]] && cp -f "${B}/release/dogecoin-tx.exe" "${STAGE}/bin/" || true
cp -f "${SRC}/COPYING" "${STAGE}/COPYING.txt" 2>/dev/null || true
cp -f "${B}/release/BUILD_STAMP.txt" "${STAGE}/"
echo "Dogecoin Core Pro ${VERSION} FULL WIN64" > "${STAGE}/CORE_PRO.txt"
ZIP="${OUT}/${REL}.zip"
rm -f "${ZIP}"
( cd "${B}/release-staging" && find "${REL}" -type f | sort | zip -X@ "${ZIP}" )
ls -lh "${ZIP}"

VERSION="${VERSION}" bash "${SRC}/scripts/make-setup-1.14.101.sh"
(
  cd "${OUT}"
  sha256sum "${REL}.zip" "${REL}-setup.exe" 2>/dev/null | tee SHA256SUMS-win64.txt
)
ls -lah "${OUT}"/${REL}*
echo FULL_RELEASE_102_OK
