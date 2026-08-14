#!/usr/bin/env bash
# Build dogecoin-core-pro 1.14.105 .deb (daemon on GPEDevBox1), sign, rsync apt.
# Secrets via env only — never commit:
#   DEV_PASS   root password for 66.42.93.123
# Does not touch gpednode1 (45.76.19.48 dump/snapshot producer).
set -euo pipefail
export PATH="/usr/bin:/bin:/usr/local/bin:${PATH:-}"

DEV_HOST="${DEV_HOST:-66.42.93.123}"
PROXY_HOST="${PROXY_HOST:-45.76.248.250}"
VERSION="${VERSION:-1.14.105}"
SRC="/mnt/c/dogedev"
GPE="/mnt/c/dogedevGPEnode"
KEY="${HOME}/.ssh/gpe_operator"
export GNUPGHOME="${GNUPGHOME:-${HOME}/.gnupg-apt-gpe}"

log() { echo "[apt-105 $(date +%H:%M:%S)] $*"; }

SP="${SSHPASS_BIN:-}"
if [[ -z "$SP" ]]; then
  if command -v sshpass >/dev/null 2>&1; then
    SP=$(command -v sshpass)
  elif [[ -x /tmp/sshpass-extract/usr/bin/sshpass ]]; then
    SP=/tmp/sshpass-extract/usr/bin/sshpass
  else
    cd /tmp
    apt-get download sshpass >/dev/null 2>&1 || true
    dpkg-deb -x sshpass_*.deb /tmp/sshpass-extract 2>/dev/null || true
    SP=/tmp/sshpass-extract/usr/bin/sshpass
  fi
fi
[[ -x "$SP" ]] || { echo "need sshpass"; exit 1; }
[[ -n "${DEV_PASS:-}" ]] || { echo "set DEV_PASS"; exit 1; }

mkdir -p ~/.ssh
chmod 700 ~/.ssh
if [[ ! -f "$KEY" && -f /mnt/c/dogedevGPEnode/local/ssh/id_ed25519_operator_gpednode1 ]]; then
  cp -f /mnt/c/dogedevGPEnode/local/ssh/id_ed25519_operator_gpednode1 "$KEY"
  chmod 600 "$KEY"
fi

ssh_dev() {
  SSHPASS="$DEV_PASS" "$SP" -e ssh -o StrictHostKeyChecking=accept-new -o ConnectTimeout=25 \
    -o PreferredAuthentications=password -o PubkeyAuthentication=no "root@${DEV_HOST}" "$@"
}
scp_dev() {
  SSHPASS="$DEV_PASS" "$SP" -e scp -o StrictHostKeyChecking=accept-new \
    -o PreferredAuthentications=password -o PubkeyAuthentication=no "$@"
}

log "Probe DEV only (not dump node)"
ssh_dev 'hostname; nproc; free -h | head -2; df -h / | tail -1
ls -la /opt/build104/src/dogecoind /opt/build105/src/dogecoind 2>/dev/null || true
/opt/build104/src/dogecoind -version 2>/dev/null | head -1 || true
'

log "Ensure build deps on DEV"
ssh_dev 'export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq build-essential libtool autotools-dev automake pkg-config bsdmainutils python3 \
  libssl-dev libevent-dev libboost-all-dev libdb-dev libdb++-dev libminiupnpc-dev libzmq3-dev \
  git curl ca-certificates 2>&1 | tail -6
'

log "Upload 1.14.105 daemon sources (no qt / no dump node)"
rm -f /tmp/doge105-src.tgz
tar -C /mnt/c/dogedev -czf /tmp/doge105-src.tgz \
  --exclude='.git' --exclude='src/qt' --exclude='depends' --exclude='release' \
  --exclude='*.o' --exclude='*.a' --exclude='*.lo' --exclude='*.la' --exclude='*.Po' \
  --exclude='*.Plo' --exclude='*.obj' --exclude='.deps' --exclude='.libs' \
  --exclude='*.exe' --exclude='autom4te.cache' \
  src configure.ac Makefile.am autogen.sh share build-aux \
  libdogecoinconsensus.pc.in contrib qa/pull-tester deploy/debian doc/install-roles.md \
  COPYING
ls -lh /tmp/doge105-src.tgz
scp_dev /tmp/doge105-src.tgz "root@${DEV_HOST}:/tmp/doge105-src.tgz"

log "Configure + make dogecoind 1.14.105 on DEV"
ssh_dev 'set -e
export PATH=/usr/bin:$PATH
# Incremental: reuse 104 object tree if present (same 1.14 DNA).
if [[ ! -d /opt/build105/src ]]; then
  if [[ -d /opt/build104/src ]]; then
    echo "seed /opt/build105 from /opt/build104"
    cp -a /opt/build104 /opt/build105
  else
    mkdir -p /opt/build105
  fi
fi
tar -xzf /tmp/doge105-src.tgz -C /opt/build105
cd /opt/build105
chmod +x autogen.sh || true
if [[ ! -x configure ]]; then
  ./autogen.sh 2>&1 | tail -20
fi
if [[ ! -f Makefile ]]; then
  ./configure --prefix=/opt/gpe-105 --disable-bench --disable-tests --with-gui=no 2>&1 | tail -25
fi
# Stamp 105 even if configure regenerated 104
sed -i "s/#define CLIENT_VERSION_REVISION [0-9]*/#define CLIENT_VERSION_REVISION 105/" src/config/dogecoin-config.h
sed -i "s/1.14.104/1.14.105/g" src/config/dogecoin-config.h
cd src
make dogecoind dogecoin-cli -j$(nproc) 2>&1 | tail -50
./dogecoind -version | head -2
ls -lh dogecoind dogecoin-cli
'

log "Fetch Linux daemon binaries"
mkdir -p /tmp/apt105-bin
scp_dev "root@${DEV_HOST}:/opt/build105/src/dogecoind" /tmp/apt105-bin/dogecoind
scp_dev "root@${DEV_HOST}:/opt/build105/src/dogecoin-cli" /tmp/apt105-bin/dogecoin-cli
chmod +x /tmp/apt105-bin/dogecoind /tmp/apt105-bin/dogecoin-cli
/tmp/apt105-bin/dogecoind -version | head -2 || true
ls -lh /tmp/apt105-bin/

OPS=""
TUI=""
TRAY=""
for p in \
  "${GPE}/gpenode-ops/gpenode-ops-linux-amd64" \
  "${GPE}/gpenode-ops/gpenode-ops"
do
  [[ -x "$p" ]] && file "$p" | grep -q ELF && OPS="$p" && break
done
for p in \
  "${GPE}/gpenode-tui/gpenode-tui-linux-amd64" \
  "${GPE}/gpenode-tui/gpenode-tui"
do
  [[ -x "$p" ]] && file "$p" | grep -q ELF && TUI="$p" && break
done
for p in "${GPE}/gpenode-tray/gpenode-tray" "${GPE}/gpenode-tray/gpenode-tray-linux"; do
  [[ -x "$p" ]] && file "$p" | grep -q ELF && TRAY="$p" && break
done
[[ -n "$OPS" ]] && cp -a "$OPS" /tmp/apt105-bin/gpenode-ops
[[ -n "$TUI" ]] && cp -a "$TUI" /tmp/apt105-bin/gpenode-tui
[[ -n "$TRAY" ]] && cp -a "$TRAY" /tmp/apt105-bin/gpenode-tray
chmod +x /tmp/apt105-bin/* || true
ls -lh /tmp/apt105-bin/

log "Build dogecoin-core-pro .deb locally"
export BIN_DIR=/tmp/apt105-bin
export VERSION
export OUT_DIR="${SRC}/release/debian"
mkdir -p "$OUT_DIR"
sed -i 's/\r$//' "${SRC}/deploy/debian/"*.sh "${SRC}/deploy/debian/dogecoin-core-pro" 2>/dev/null || true
bash "${SRC}/deploy/debian/build-deb.sh"
DEB="${OUT_DIR}/dogecoin-core-pro_${VERSION}-1_amd64.deb"
[[ -f "$DEB" ]] || { echo "missing $DEB"; exit 1; }
ls -lh "$DEB"

log "Build transitional dogecoin-gpenode metapackage"
TRANS=/tmp/gpenode-trans-$$
rm -rf "$TRANS"
mkdir -p "$TRANS/DEBIAN"
cat > "$TRANS/DEBIAN/control" <<EOF
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
 GPENode is now Dogecoin Core Pro. This empty package pulls
 dogecoin-core-pro so existing apt install dogecoin-gpenode still works.
EOF
TRANS_DEB="${OUT_DIR}/dogecoin-gpenode_${VERSION}-1_all.deb"
rm -f "$TRANS_DEB"
if command -v fakeroot >/dev/null 2>&1; then
  fakeroot dpkg-deb --build "$TRANS" "$TRANS_DEB"
else
  dpkg-deb --build "$TRANS" "$TRANS_DEB"
fi
rm -rf "$TRANS"
ls -lh "$TRANS_DEB"

log "Sign apt repo (local GPG, never on CDN)"
mkdir -p "$GNUPGHOME"
chmod 700 "$GNUPGHOME"
GPG_KEY_ID=$(gpg --list-secret-keys --with-colons 2>/dev/null | awk -F: '/^fpr:/{print $10; exit}' || true)
if [[ -z "$GPG_KEY_ID" ]]; then
  echo "ERROR: no secret key in GNUPGHOME=$GNUPGHOME — will not rotate the live pubkey"
  exit 1
fi
log "GPG_KEY_ID=$GPG_KEY_ID"

REPO=/tmp/apt-repo-105
rm -rf "$REPO"
mkdir -p "$REPO/pool/main" "$REPO/dists/stable/main/binary-amd64"
cp -a "$DEB" "$TRANS_DEB" "$REPO/pool/main/"
# Keep prior versions so old pins still resolve
for old in \
  dogecoin-core-pro_1.14.104-1_amd64.deb \
  dogecoin-gpenode_1.14.104-1_all.deb \
  dogecoin-gpenode_1.14.103-1_amd64.deb \
  dogecoin-gpenode_1.14.102-1_amd64.deb
do
  curl -fsS -o "$REPO/pool/main/${old}" \
    "https://apt.dogecli.gopastearth.com/pool/main/${old}" || true
done
# Prefer local 104 debs if curl missed them
if [[ -f "${SRC}/release/debian/dogecoin-core-pro_1.14.104-1_amd64.deb" ]]; then
  cp -a "${SRC}/release/debian/dogecoin-core-pro_1.14.104-1_amd64.deb" "$REPO/pool/main/"
fi
if [[ -f "${SRC}/release/debian/dogecoin-gpenode_1.14.104-1_all.deb" ]]; then
  cp -a "${SRC}/release/debian/dogecoin-gpenode_1.14.104-1_all.deb" "$REPO/pool/main/"
fi

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
<p><strong>Latest:</strong> <code>dogecoin-core-pro</code> <strong>${VERSION}-1</strong> (replaces <code>dogecoin-gpenode</code>)</p>
<pre>
curl -fsSL https://apt.dogecli.gopastearth.com/pubkey.gpg | sudo gpg --dearmor -o /usr/share/keyrings/gpenode.gpg
echo "deb [signed-by=/usr/share/keyrings/gpenode.gpg] https://apt.dogecli.gopastearth.com stable main" | sudo tee /etc/apt/sources.list.d/gpenode.list
sudo apt update
sudo apt install dogecoin-core-pro
# or: sudo apt install dogecoin-gpenode   # transitional, pulls Core Pro
</pre>
<p>Same mainnet consensus. One dogecoind. Client / Server / Hybrid asked on first install.</p>
<p>Testnet: <code>dogecoin-core-pro --testnet</code> or the <strong>Dogecoin Core Pro Testnet</strong> menu entry.</p>
</body></html>
HTML

log "Rsync public tree to proxybox (key auth, no private keys). Not the dump node."
rsync -av \
  -e "ssh -i $KEY -o StrictHostKeyChecking=accept-new -o BatchMode=yes" \
  "$REPO/" "root@${PROXY_HOST}:/var/www/gpenode-apt/"

log "PUBLIC VERIFY"
curl -fsS https://apt.dogecli.gopastearth.com/dists/stable/main/binary-amd64/Packages | grep -E '^(Package|Version|Filename):'
log "APT_1.14.105_OK"
