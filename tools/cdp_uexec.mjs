// usage: node cdp_uexec.mjs "<console command>" <tabID-prefix> [cdpPort]
const CMD = process.argv[2]; const PREFIX = process.argv[3] || ''; const PORT = parseInt(process.argv[4] || '9333', 10);
const tabs = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const tab = tabs.find(t => t.id.startsWith(PREFIX));
if (!tab) { console.log('tab not found'); process.exit(1); }
const ws = new WebSocket(tab.webSocketDebuggerUrl);
let id = 1; const pending = new Map();
const send = (m, p = {}) => new Promise(res => { const i = id++; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: m, params: p })); });
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
ws.addEventListener('message', ev => { const m = JSON.parse(ev.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); } });
const expr = `(() => { const s = ${JSON.stringify(CMD)}; const n = lengthBytesUTF8(s) + 1; const p = _malloc(n); stringToUTF8(s, p, n); const r = Module._wndr_exec(BigInt(p)); _free(p); return String(r); })()`;
const r = await send('Runtime.evaluate', { expression: expr, returnByValue: true });
console.log(JSON.stringify(r.result?.value ?? r.exceptionDetails?.exception?.description ?? r));
process.exit(0);
