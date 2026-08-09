#!/usr/bin/env bash
set -uo pipefail
export PATH=/usr/bin:/bin
IMG='-iolmTBEnbRh7ASQ.png'
for u in \
  "https://memestream.gopastearth.com/media/memestream/${IMG}" \
  "https://gopastearth.com/media/memestream/${IMG}" \
  "https://www.gopastearth.com/media/memestream/${IMG}" \
  "https://gopastearth.com/api/public/memestream/media/${IMG}" \
  "https://gopastearth.com/api/media/memestream/${IMG}" \
  "https://staging.gopastearth.com/media/memestream/${IMG}"
do
  echo "=== $u"
  curl -sSI --max-time 12 "$u" 2>&1 | head -12
  code=$(curl -sS -o /tmp/probe.bin -w '%{http_code} %{content_type} %{size_download}' --max-time 12 "$u" 2>/dev/null || echo fail)
  echo "body: $code"
  file /tmp/probe.bin 2>/dev/null || true
  echo
done
# also try feed imageUrl as returned
python3 - <<'PY'
import json,urllib.request
u='https://gopastearth.com/api/public/memestream/feed?limit=3'
with urllib.request.urlopen(u, timeout=15) as r:
    d=json.load(r)
for it in d.get('items',[])[:3]:
    print('item', it.get('title'), 'imageUrl=', it.get('imageUrl'))
PY
