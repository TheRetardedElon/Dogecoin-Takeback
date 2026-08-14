#!/usr/bin/env bash
# Replace the 1.14.105 .deb that accidentally packed an old 1.14.102 dogecoind.
# Reuse the published 1.14.104 Linux daemon + 105 TUI/GUI/launcher.
# Does not touch 45.76.19.48.
set -euo pipefail
export PATH="/usr/bin:/bin:/usr/local/bin:${PATH:-}"
export GNUPGHOME="${GNUPGHOME:-${HOME}/.gnupg-apt-gpe}"
SRC=/mnt/c/dogedev
GPE=/mnt/c/dogedevGPEnode
VERSION=1.14.105
PROXY_HOST="${PROXY_HOST:-45.76.248.250}"
KEY="${HOME}/.ssh/gpe_operator"
OUT="${SRC}/release/debian"

log() { echo "[fix-105 $(date +%H:%M:%S)] $*"; }

DEB104="${OUT}/dogecoin-core-pro_1.14.104-1_amd64.deb"
[[ -f "$DEB104" ]] || { echo "missing $DEB104"; exit 1; }

EX=/tmp/deb104-extract
rm -rf "$EX"
mkdir -p "$EX"
dpkg-deb -x "$DEB104" "$EX"
D="$EX/usr/lib/dogecoin-core-pro"
[[ -x "$D/dogecoind" && -x "$D/dogecoin-cli" ]] || { echo "104 deb missing daemon"; exit 1; }
log "104 daemon bytes $(stat -c%s "$D/dogecoind") (reuse published 1.14.104 Linux node)"

BIN=/tmp/apt105-good
rm -rf "$BIN"
mkdir -p "$BIN"
cp -a "$D/dogecoind" "$D/dogecoin-cli" "$BIN/"
# helpers: 105 TUI, existing linux ops/tray
cp -a "${GPE}/gpenode-tui/gpenode-tui-linux-amd64" "$BIN/gpenode-tui"
if [[ -x "${GPE}/gpenode-ops/gpenode-ops-linux-amd64" ]]; then
  cp -a "${GPE}/gpenode-ops/gpenode-ops-linux-amd64" "$BIN/gpenode-ops"
fi
for p in "${GPE}/gpenode-tray/gpenode-tray" "${GPE}/gpenode-tray/gpenode-tray-linux"; do
  if [[ -x "$p" ]] && file "$p" | grep -q ELF; then
    cp -a "$p" "$BIN/gpenode-tray"
    break
  fi
done
# Boost/libs from the 104 package
LIB_DIR=/tmp/apt105-libs
rm -rf "$LIB_DIR"
mkdir -p "$LIB_DIR"
cp -a "$D"/*.so* "$LIB_DIR/" 2>/dev/null || true
chmod +x "$BIN"/* || true
ls -lh "$BIN"

log "Rebuild 105 .deb"
export BIN_DIR="$BIN"
export LIB_DIR
export VERSION
export OUT_DIR="$OUT"
sed -i 's/\r$//' "${SRC}/deploy/debian/"*.sh "${SRC}/deploy/debian/dogecoin-core-pro"
bash "${SRC}/deploy/debian/build-deb.sh"
DEB="${OUT}/dogecoin-core-pro_${VERSION}-1_amd64.deb"
TRANS="${OUT}/dogecoin-gpenode_${VERSION}-1_all.deb"
[[ -f "$DEB" ]]

# Verify packed daemon matches 104 + new 105 surfaces
CHK=/tmp/deb105-check
rm -rf "$CHK"
mkdir -p "$CHK"
dpkg-deb -x "$DEB" "$CHK"
cmp -s "$D/dogecoind" "$CHK/usr/lib/dogecoin-core-pro/dogecoind"
test -f "$CHK/usr/share/applications/dogecoin-core-pro-testnet.desktop"
test -f "$CHK/usr/share/pixmaps/dogecoin-testnet.png"
test -x "$CHK/usr/bin/dogecoin-core-pro"
test -x "$CHK/usr/bin/gpenode-tui"
test -x "$CHK/usr/bin/dogecoin-pro-gui"
grep -q -- '--testnet' "$CHK/usr/bin/dogecoin-core-pro"
log "DEB contents OK"

# Transitional if missing
if [[ ! -f "$TRANS" ]]; then
  T=/tmp/gpenode-trans-105
  rm -rf "$T"
  mkdir -p "$T/DEBIAN"
  cat > "$T/DEBIAN/control" <<EOF
Package: dogecoin-gpenode
Version: ${VERSION}-1
Section: net
Priority: optional
Architecture: all
Depends: dogecoin-core-pro (>= ${VERSION})
Replaces: dogecoin-gpenode
Provides: dogecoin-gpenode
Maintainer: The Retarded Elon <noreply@gopastearth.com>
Description: transitional package to dogecoin-core-pro
EOF
  dpkg-deb --build "$T" "$TRANS"
fi

log "Resign repo"
GPG_KEY_ID=$(gpg --list-secret-keys --with-colons | awk -F: '/^fpr:/{print $10; exit}')
[[ -n "$GPG_KEY_ID" ]]
REPO=/tmp/apt-repo-105
rm -rf "$REPO"
mkdir -p "$REPO/pool/main" "$REPO/dists/stable/main/binary-amd64"
cp -a "$DEB" "$TRANS" "$REPO/pool/main/"
# keep prior
for old in \
  "${OUT}/dogecoin-core-pro_1.14.104-1_amd64.deb" \
  "${OUT}/dogecoin-gpenode_1.14.104-1_all.deb"
do
  [[ -f "$old" ]] && cp -a "$old" "$REPO/pool/main/"
done
for old in \
  dogecoin-gpenode_1.14.103-1_amd64.deb \
  dogecoin-gpenode_1.14.102-1_amd64.deb
do
  curl -fsS -o "$REPO/pool/main/${old}" \
    "https://apt.dogecli.gopastearth.com/pool/main/${old}" || true
done

cd "$REPO"
dpkg-scanpackages --multiversion pool/main /dev/null > dists/stable/main/binary-amd64/Packages
gzip -9c dists/stable/main/binary-amd64/Packages > dists/stable/main/binary-amd64/Packages.gz
{
  echo "Origin: GPE Dogecoin"
  echo "Label: dogecoin-core-pro"
  echo "Suite: stable"
  echo "Codename: stable"
  echo "Architectures: amd64 all"
  echo "Components: main"
  echo "Description: Dogecoin Core Pro / GPENode apt repository"
  echo "Date: $(date -Ru)"
  echo "SHA256:"
  for f in main/binary-amd64/Packages main/binary-amd64/Packages.gz; do
    path="dists/stable/$f"
    size=$(wc -c < "$path" | tr -d ' ')
    sha=$(sha256sum "$path" | awk '{print $1}')
    printf " %s %8s %s\n" "$sha" "$size" "$f"
  done
} > dists/stable/Release
gpg --default-key "$GPG_KEY_ID" --batch --yes --armor --detach-sign -o dists/stable/Release.gpg dists/stable/Release
gpg --default-key "$GPG_KEY_ID" --batch --yes --clearsign -o dists/stable/InRelease dists/stable/Release
gpg --export --armor "$GPG_KEY_ID" > pubkey.gpg
cat > index.html <<HTML
<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"/><title>Dogecoin Core Pro apt</title></head>
<body>
<h1>Dogecoin Core Pro / GPENode apt</h1>
<p><strong>Latest:</strong> <code>dogecoin-core-pro</code> <strong>${VERSION}-1</strong></p>
<pre>
curl -fsSL https://apt.dogecli.gopastearth.com/pubkey.gpg | sudo gpg --dearmor -o /usr/share/keyrings/gpenode.gpg
echo "deb [signed-by=/usr/share/keyrings/gpenode.gpg] https://apt.dogecli.gopastearth.com stable main" | sudo tee /etc/apt/sources.list.d/gpenode.list
sudo apt update
sudo apt install dogecoin-core-pro
</pre>
<p>Testnet: <code>dogecoin-core-pro --testnet</code> or the <strong>Dogecoin Core Pro Testnet</strong> menu entry.</p>
</body></html>
HTML

log "Rsync to proxy (not dump node)"
rsync -av \
  -e "ssh -i $KEY -o StrictHostKeyChecking=accept-new -o BatchMode=yes" \
  "$REPO/" "root@${PROXY_HOST}:/var/www/gpenode-apt/"

log "PUBLIC VERIFY"
curl -fsS https://apt.dogecli.gopastearth.com/dists/stable/main/binary-amd64/Packages | grep -E '^(Package|Version|Filename):'
sha256sum "$DEB"
log "FIX_105_OK"
