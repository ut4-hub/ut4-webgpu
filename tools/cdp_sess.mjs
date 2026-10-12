// usage: node cdp_sess.mjs <tabPrefix> <method> [paramsJSON] — via browser endpoint, flattened session
const [PREFIX, METHOD, P] = process.argv.slice(2);
const v = await (await fetch('http://127.0.0.1:9333/json/version')).json();
const ws = new WebSocket(v.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
let id = 1; const pending = new Map();
const send = (m, p = {}, sessionId) => new Promise(res => { const i = id++; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: m, params: p, sessionId })); });
ws.addEventListener('message', ev => { const m = JSON.parse(ev.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); } });
setTimeout(() => { console.log('timeout'); process.exit(2); }, 15000);
const tg = (await send('Target.getTargets')).result.targetInfos.find(t => t.targetId.startsWith(PREFIX));
const s = (await send('Target.attachToTarget', { targetId: tg.targetId, flatten: true })).result.sessionId;
const r = await send(METHOD, JSON.parse(P || '{}'), s);
console.log(JSON.stringify(r).slice(0, 400)); process.exit(0);
