// One-shot: pause every worker, sample its stack, report which one is parked where.
// usage: node cdp_stacks.mjs [cdpPort]
// Uses flatten-attach to each worker target, Debugger.enable + pause, then reads
// callFrames from the paused event. Safe: resumes and detaches immediately.
const PORT = parseInt(process.argv[2] || '9333', 10);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const tabs = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const page = tabs.find(t => t.url.includes('127.0.0.1:8091'));
if (!page) { console.error('game page not found'); process.exit(1); }
const ws = new WebSocket(page.webSocketDebuggerUrl);
let id = 1; const pending = new Map(); const events = [];
const send = (m, p = {}, sessionId) => new Promise(res => {
  const i = id++;
  pending.set(i, res);
  ws.send(JSON.stringify({ id: i, method: m, params: p, sessionId }));
});
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
ws.addEventListener('message', ev => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); return; }
  if (m.method) events.push(m);
});
const { targetInfos } = await send('Target.getTargets');
const workers = targetInfos.filter(t => t.type === 'worker');
console.log(`found ${workers.length} workers`);
for (const w of workers) {
  const { sessionId } = await send('Target.attachToTarget', { targetId: w.targetId, flatten: true });
  await send('Debugger.enable', {}, sessionId).catch(() => {});
  const pauseDone = send('Debugger.pause', {}, sessionId).catch(() => {});
  await sleep(300);
  const pausedEv = events.filter(e => e.method === 'Debugger.paused' && e.sessionId === sessionId).pop();
  if (pausedEv) {
    const frames = pausedEv.params.callFrames.slice(0, 4).map(f => {
      const url = (f.url || '').split('/').pop() || '(vm)';
      const fn = f.functionName || '(anon)';
      return `${fn}@${url}:${f.location.lineNumber}:${f.location.columnNumber}`;
    });
    console.log(`worker ${w.targetId.slice(0, 8)}: ${frames.join(' | ')}`);
    await send('Debugger.resume', {}, sessionId).catch(() => {});
  } else {
    console.log(`worker ${w.targetId.slice(0, 8)}: no pause event (running or gone)`);
    await send('Debugger.resume', {}, sessionId).catch(() => {});
  }
  await send('Debugger.disable', {}, sessionId).catch(() => {});
  await send('Target.detachFromTarget', { sessionId }).catch(() => {});
}
process.exit(0);
