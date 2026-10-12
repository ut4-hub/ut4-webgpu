// node cdp_wprof2.mjs <tab> <secs> <funcSubstring>: callers (up to 4 levels) of a hot function on the busiest thread
const [PREFIX, SECS, FN] = [process.argv[2], parseFloat(process.argv[3] || '5'), process.argv[4]];
const v = await (await fetch('http://127.0.0.1:9333/json/version')).json();
const ws = new WebSocket(v.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
let id = 1; const pending = new Map();
const send = (m, p = {}, sessionId) => new Promise(res => { const i = id++; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: m, params: p, sessionId })); });
const attached = [];
ws.addEventListener('message', ev => { const m = JSON.parse(ev.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); } if (m.method === 'Target.attachedToTarget') attached.push(m.params); });
const page = (await send('Target.getTargets')).result.targetInfos.find(t => t.targetId.startsWith(PREFIX));
const ps = (await send('Target.attachToTarget', { targetId: page.targetId, flatten: true })).result.sessionId;
await send('Target.setAutoAttach', { autoAttach: true, waitForDebuggerOnStart: false, flatten: true }, ps);
await new Promise(r => setTimeout(r, 1500));
const sessions = attached.filter(a => a.targetInfo.type === 'worker').map(a => a.sessionId);
await Promise.all(sessions.map(async s => { await send('Profiler.enable', {}, s); await send('Profiler.setSamplingInterval', { interval: 500 }, s); await send('Profiler.start', {}, s); }));
await new Promise(r => setTimeout(r, SECS * 1000));
const profs = await Promise.all(sessions.map(async s => (await send('Profiler.stop', {}, s)).result?.profile));
let best = null, bestHits = 0;
for (const p of profs) { if (!p) continue; const byId = new Map(p.nodes.map(n => [n.id, n])); let h = 0; for (const sid of p.samples) if ((byId.get(sid).callFrame.functionName || '').includes(FN)) h++; if (h > bestHits) { bestHits = h; best = p; } }
if (!best) { console.log('not found'); process.exit(0); }
const byId = new Map(best.nodes.map(n => [n.id, n])); const parent = new Map();
for (const n of best.nodes) for (const c of (n.children || [])) parent.set(c, n.id);
const chains = new Map(); let tot = 0;
for (const sid of best.samples) {
  let n = byId.get(sid); if (!(n.callFrame.functionName || '').includes(FN)) continue; tot++;
  const ch = []; let cur = parent.get(n.id);
  for (let d = 0; d < 6 && cur; d++) { const cn = byId.get(cur); ch.push((cn.callFrame.functionName || '?').slice(0, 90)); cur = parent.get(cur); }
  const k = ch.join('  <-  '); chains.set(k, (chains.get(k) || 0) + 1);
}
console.log('samples in', FN, tot);
for (const [k, c] of [...chains.entries()].sort((a, b) => b[1] - a[1]).slice(0, 8)) console.log(`${(100 * c / tot).toFixed(0)}%  ${k}\n`);
process.exit(0);
