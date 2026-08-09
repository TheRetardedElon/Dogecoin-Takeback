#!/usr/bin/env bash
set -uo pipefail
export PATH=/usr/bin:/bin
# From live feed
python3 <<'PY'
import json, urllib.request, ssl
ctx = ssl.create_default_context()
with urllib.request.urlopen('https://gopastearth.com/api/public/memestream/feed?limit=5', context=ctx, timeout=20) as r:
    d = json.load(r)
items = d.get('items') or []
for it in items:
    print('title=', it.get('title'))
    print('  imageUrl=', it.get('imageUrl'))
    print('  image=', it.get('image'))
    print('  mediaUrl=', it.get('mediaUrl'))
    print('  keys=', sorted(it.keys()))
    print()
# dump full first item
if items:
    print('FULL0', json.dumps(items[0], indent=2)[:2000])
PY

echo '--- try common media patterns ---'
IMG='-iolmTBEnbRh7ASQ.png'
for u in \
  "https://gopastearth.com/api/public/memestream/file/${IMG}" \
  "https://gopastearth.com/api/public/memestream/image/${IMG}" \
  "https://gopastearth.com/api/public/memestream/images/${IMG}" \
  "https://gopastearth.com/uploads/memestream/${IMG}" \
  "https://gopastearth.com/static/memestream/${IMG}" \
  "https://gopastearth.com/files/memestream/${IMG}" \
  "https://gopastearth.com/api/public/memestream/media?file=${IMG}" \
  "https://gopastearth.com/api/public/memestream/media?path=/media/memestream/${IMG}"
do
  ct=$(curl -sS -o /tmp/p.bin -w '%{http_code} %{content_type} %{size_download}' --max-time 12 -H 'Accept: image/*' "$u" 2>/dev/null || echo fail)
  ft=$(file -b /tmp/p.bin 2>/dev/null | head -c 60)
  echo "$ct | $ft | $u"
done
