// node cdp_trace.mjs <secs> [focusFn]: browser-wide V8 CPU sampling via Tracing; top self-time per thread
const SECS = parseFloat(process.argv[2] || '4'), FOCUS = process.argv[3];
const v = await (await fetch('http://127.0.0.1:9333/json/version')).json();
const ws = new WebSocket(v.webSocketDebuggerUrl, { maxPayload: 1 << 30 });
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
let id = 1; const pending = new Map(); const events = []; let done;
const finished = new Promise(r => done = r);
const send = (m, p = {}) => new Promise(res => { const i = id++; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: m, params: p })); });
ws.addEventListener('message', ev => { const m = JSON.parse(ev.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
  if (m.method === 'Tracing.dataCollected') events.push(...m.params.value); if (m.method === 'Tracing.tracingComplete') done(); });
const r0 = await send('Tracing.start', { traceConfig: { includedCategories: ['disabled-by-default-v8.cpu_profiler'], recordMode: 'recordContinuously' }, transferMode: 'ReportEvents' });
if (r0.error) { console.log('start error', JSON.stringify(r0.error)); process.exit(1); }
await new Promise(r => setTimeout(r, SECS * 1000));
await send('Tracing.end'); await finished;
console.log("events", events.length, [...new Set(events.map(e=>e.name))].slice(0,20).join(","));
// group ProfileChunk by (pid, tid/profile id)
const profs = new Map();
for (const e of events) {
  if (e.name !== 'ProfileChunk') continue;
  const key = e.pid + ':' + e.id; let P = profs.get(key); if (!P) { P = { nodes: new Map(), samples: [], deltas: [], tid: e.tid }; profs.set(key, P); }
  const d = e.args.data || {}; const cp = d.cpuProfile || {};
  for (const n of (cp.nodes || [])) P.nodes.set(n.id, n);
  if (cp.samples) P.samples.push(...cp.samples); if (d.timeDeltas) P.deltas.push(...d.timeDeltas);
}
for (const [k, P] of profs) {
  for (const n of P.nodes.values()) if (n.parent) n._p = n.parent;
  const self = new Map(); let tot = 0, idle = 0;
  for (let i = 0; i < P.samples.length; i++) { const n = P.nodes.get(P.samples[i]); if (!n) continue; const dt = P.deltas[i] || 0; tot += dt; const fn = n.callFrame.functionName || '(anon)';
    if (/\(idle\)|\(program\)|futex_wait|cond_timedwait|atomics_sleep|Atomics\.wait/.test(fn)) idle += dt; self.set(fn, (self.get(fn) || 0) + dt); }
  if (tot === 0 || (tot - idle) / tot < 0.2) continue;
  console.log(`\n== ${k} tid=${P.tid} busy ${(100 * (tot - idle) / tot).toFixed(0)}% of ${(tot / 1e6).toFixed(1)}s`);
  for (const [fn, t] of [...self.entries()].sort((a, b) => b[1] - a[1]).slice(0, 16)) console.log(`  ${(100 * t / tot).toFixed(1).padStart(5)}%  ${fn.slice(0, 110)}`);
  if (FOCUS) { const chains = new Map(); let ft = 0;
    for (const sid of P.samples) { let n = P.nodes.get(sid); if (!n || !(n.callFrame.functionName || '').includes(FOCUS)) continue; ft++; const ch = []; let c = n._p; for (let d = 0; d < 6 && c; d++) { const cn = P.nodes.get(c); if (!cn) break; ch.push((cn.callFrame.functionName || '?').slice(0, 80)); c = cn._p; } const kk = ch.join(' <- '); chains.set(kk, (chains.get(kk) || 0) + 1); }
    if (ft) { console.log(`  callers of ${FOCUS}:`); for (const [kk, c] of [...chains.entries()].sort((a, b) => b[1] - a[1]).slice(0, 6)) console.log(`    ${(100 * c / ft).toFixed(0)}% ${kk}`); } }
}
process.exit(0);
