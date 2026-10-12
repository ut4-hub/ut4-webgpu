// usage: node cdp_rawnav.mjs <url> <tabID-prefix> [port] — Page.navigate without waiting for load
const URL_ = process.argv[2], PREFIX = process.argv[3], PORT = parseInt(process.argv[4] || '9333', 10);
const tabs = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const tab = tabs.find(t => t.id.startsWith(PREFIX));
const ws = new WebSocket(tab.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
const t = setTimeout(() => { console.log('timeout waiting for reply'); process.exit(2); }, 15000);
ws.addEventListener('message', ev => { console.log(String(ev.data).slice(0, 200)); clearTimeout(t); process.exit(0); });
ws.send(JSON.stringify({ id: 1, method: process.env.M, params: JSON.parse(process.env.P || '{}') }));
