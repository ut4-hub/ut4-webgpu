#!/bin/sh
cd /usr/share/nginx/html
for e in UnrealTournamentwasm64.wasm:223144118 UnrealTournament.js:12433012 index.hub.html:13157; do
  f=${e%%:*}; sz=${e##*:}; out=$f; [ "$f" = index.hub.html ] && out=index.html
  wget -q -O "$out.part" "http://192.168.69.20:8099/$f" && [ "$(stat -c %s "$out.part")" = "$sz" ] && mv "$out.part" "$out" && echo "ok $out" || echo "FAIL $out"
done
chmod 644 *.js *.wasm *.html 2>/dev/null
echo FETCH-DONE
