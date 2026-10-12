// which worker threads answer a trivial Runtime.evaluate within 5s?
const PREFIX = process.argv[2];
const v = await (await fetch('http://127.0.0.1:9333/json/version')).json();
const ws = new WebSocket(v.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
let id = 1; const pending = new Map(); const attached = [];
const send = (m, p = {}, sessionId) => new Promise(res => { const i = id++; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: m, params: p, sessionId })); });
ws.addEventListener('message', ev => { const m = JSON.parse(ev.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); } if (m.method === 'Target.attachedToTarget') attached.push(m.params); });
const page = (await send('Target.getTargets')).result.targetInfos.find(t => t.targetId.startsWith(PREFIX));
const ps = (await send('Target.attachToTarget', { targetId: page.targetId, flatten: true })).result.sessionId;
await send('Target.setAutoAttach', { autoAttach: true, waitForDebuggerOnStart: false, flatten: true }, ps);
await new Promise(r => setTimeout(r, 1500));
const to = (p, ms) => Promise.race([p, new Promise(r => setTimeout(() => r(null), ms))]);
const ws_ = attached.filter(a => a.targetInfo.type === 'worker');
const r = await Promise.all(ws_.map(async a => { const t0 = Date.now(); const x = await to(send('Runtime.evaluate', { expression: '(typeof _pthread_self!=="undefined"? Number(_pthread_self()):0)+"|"+(globalThis.name||"")', returnByValue: true }, a.sessionId), 5000); return [a.targetInfo.targetId.slice(0, 6), x ? x.result?.result?.value : 'NO-ANSWER', Date.now() - t0]; }));
for (const x of r) console.log(x.join('  '));
process.exit(0);
