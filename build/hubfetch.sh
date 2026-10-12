#!/bin/sh
cd /usr/share/nginx/html
rm -f ._* .w *.part
for e in  ContentBootstrap.data:91490645 ContentBootstrap.js:196510 ContentBootstrap_1.data:919650529 ContentBootstrap_1.js:9374 ContentBootstrap_2.data:1191692098 ContentBootstrap_2.js:9380 ContentBootstrap_3.data:1368568440 ContentBootstrap_3.js:9380 ContentBootstrap_4.data:1000440414 ContentBootstrap_4.js:9378 ContentBootstrap_5.data:623578385 ContentBootstrap_5.js:9376 ContentBootstrap_6.data:916179538 ContentBootstrap_6.js:9376 ContentBootstrap_7.data:647109275 ContentBootstrap_7.js:9376 ContentBootstrap_8.data:611013208 ContentBootstrap_8.js:9376 ContentBootstrap_9.data:24368896 ContentBootstrap_9.js:9372 ContentBootstrap_10.data:8256392 ContentBootstrap_10.js:12148 UnrealTournamentwasm64.data:22073525 UnrealTournamentwasm64.wasm:223128545 UnrealTournament.js:12432458; do
  f=${e%%:*}; sz=${e##*:}
  have=$(stat -c %s "$f" 2>/dev/null || echo 0)
  if [ "$have" = "$sz" ] && [ "${f##*.}" = "data" ]; then echo "skip $f"; continue; fi
  wget -q -O "$f.part" "http://192.168.69.20:8099/$f" && [ "$(stat -c %s "$f.part")" = "$sz" ] && mv "$f.part" "$f" && echo "ok $f" || echo "FAIL $f"
done
wget -q -O index.html.part "http://192.168.69.20:8099/index.hub.html" && [ "$(stat -c %s index.html.part)" = "12746" ] && mv index.html.part index.html && echo "ok index.html" || echo "FAIL index.html"
chmod 644 *.data *.js *.wasm *.html 2>/dev/null
echo FETCH-DONE
