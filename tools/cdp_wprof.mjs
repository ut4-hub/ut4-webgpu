// node cdp_wprof.mjs <pageTabPrefix> [seconds]: CPU-profile the page + all its workers, print top self-time per thread
const [PREFIX, SECS, FOCUS] = [process.argv[2], parseFloat(process.argv[3] || '5'), process.argv[4]];
const v = await (await fetch('http://127.0.0.1:9333/json/version')).json();
const ws = new WebSocket(v.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
let id = 1; const pending = new Map();
const send = (m, p = {}, sessionId) => new Promise(res => { const i = id++; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: m, params: p, sessionId })); });
ws.addEventListener('message', ev => { const m = JSON.parse(ev.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); } });
const page = (await send('Target.getTargets')).result.targetInfos.find(t => t.targetId.startsWith(PREFIX));
const ps = (await send('Target.attachToTarget', { targetId: page.targetId, flatten: true })).result.sessionId;
// discover workers via auto-attach on the page session
const sessions = [{ name: 'main', s: ps }];
const attached = [];
ws.addEventListener('message', ev => { const m = JSON.parse(ev.data); if (m.method === 'Target.attachedToTarget') attached.push(m.params); });
await send('Target.setAutoAttach', { autoAttach: true, waitForDebuggerOnStart: false, flatten: true }, ps);
await new Promise(r => setTimeout(r, 1500));
for (const a of attached) if (a.targetInfo.type === 'worker') sessions.push({ name: a.targetInfo.title || a.targetInfo.targetId.slice(0, 6), s: a.sessionId });
console.log('threads:', sessions.length, new Date().toISOString());
const to = (p, ms) => Promise.race([p, new Promise(r => setTimeout(() => r(null), ms))]);
await Promise.all(sessions.map(async x => { x.ok = !!(await to((async () => { await send('Profiler.enable', {}, x.s); await send('Profiler.setSamplingInterval', { interval: 1000 }, x.s); await send('Profiler.start', {}, x.s); return 1; })(), 20000)); }));
console.log('profiling', sessions.filter(x => x.ok).length);
await new Promise(r => setTimeout(r, SECS * 1000));
const res = await Promise.all(sessions.filter(x => x.ok).map(async x => ({ x, p: (await to(send('Profiler.stop', {}, x.s), 60000))?.result?.profile })));
for (const { x, p } of res) {
  if (!p) continue;
  const byId = new Map(p.nodes.map(n => [n.id, n]));
  const self = new Map(); let total = 0, idle = 0;
  const dts = p.timeDeltas; 
  for (let i = 0; i < p.samples.length; i++) {
    const n = byId.get(p.samples[i]); const dt = dts[i] || 0; total += dt;
    const fn = n.callFrame.functionName || '(anon)';
    if (fn === '(idle)' || fn === '(program)' || fn.includes('Atomics.wait') || fn === 'wait') { idle += dt; }
    self.set(fn, (self.get(fn) || 0) + dt);
  }
  const busy = total - idle;
  if (busy / total < 0.05) continue;
  const top = [...self.entries()].sort((a, b) => b[1] - a[1]).slice(0, 14);
  console.log(`\n== ${x.name} busy ${(100 * busy / total).toFixed(0)}% of ${(total / 1e6).toFixed(1)}s`);
  for (const [fn, t] of top) console.log(`  ${(100 * t / total).toFixed(1).padStart(5)}%  ${fn.slice(0, 110)}`);
  { // inclusive
    const parent = new Map(); for (const n of p.nodes) for (const c of (n.children || [])) parent.set(c, n.id);
    const incl = new Map();
    for (let i = 0; i < p.samples.length; i++) { const dt = dts[i] || 0; const seen = new Set(); let cur = p.samples[i];
      while (cur) { const fn = byId.get(cur).callFrame.functionName || '(anon)'; if (!seen.has(fn)) { seen.add(fn); incl.set(fn, (incl.get(fn) || 0) + dt); } cur = parent.get(cur); } }
    console.log('  -- inclusive --');
    for (const [fn, t] of [...incl.entries()].sort((a, b) => b[1] - a[1]).slice(0, 45)) console.log(`  ${(100 * t / total).toFixed(1).padStart(5)}%  ${fn.slice(0, 120)}`);
  }
  if (FOCUS) {
    const parent = new Map(); for (const n of p.nodes) for (const c of (n.children || [])) parent.set(c, n.id);
    const chains = new Map(); let ft = 0;
    for (let i = 0; i < p.samples.length; i++) { const n = byId.get(p.samples[i]); if (!(n.callFrame.functionName || '').includes(FOCUS)) continue; ft++;
      const ch = []; let cur = parent.get(n.id); for (let d = 0; d < 6 && cur; d++) { ch.push((byId.get(cur).callFrame.functionName || '?').slice(0, 80)); cur = parent.get(cur); }
      const k = ch.join(' <- '); chains.set(k, (chains.get(k) || 0) + 1); }
    if (ft) { console.log(`  callers of ${FOCUS}:`); for (const [k, c] of [...chains.entries()].sort((a, b) => b[1] - a[1]).slice(0, 6)) console.log(`    ${(100 * c / ft).toFixed(0)}% ${k}`); }
  }
}
process.exit(0);
