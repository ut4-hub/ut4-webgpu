// One-shot: pause workers and sample stacks, with per-worker timeouts.
// usage: node cdp_stacks2.mjs [cdpPort] [maxWorkers]
const PORT = parseInt(process.argv[2] || '9333', 10);
const MAXW = parseInt(process.argv[3] || '12', 10);
const sleep = ms => new Promise(r => setTimeout(r, ms));
function withTimeout(p, ms, tag) {
  return Promise.race([p, new Promise((_, rej) => setTimeout(() => rej(new Error('timeout ' + tag)), ms))]);
}
const tabs = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const page = tabs.find(t => t.url.includes('127.0.0.1:8091'));
if (!page) { console.error('game page not found'); process.exit(1); }
const ws = new WebSocket(page.webSocketDebuggerUrl);
let id = 1; const pending = new Map(); const events = [];
const send = (m, p = {}, sessionId) => {
  const i = id++;
  return new Promise((res, rej) => {
    pending.set(i, { res, rej });
    ws.send(JSON.stringify({ id: i, method: m, params: p, sessionId }));
    setTimeout(() => { if (pending.has(i)) { pending.get(i).rej(new Error('send timeout ' + m)); pending.delete(i); } }, 5000);
  });
};
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
ws.addEventListener('message', ev => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id).res(m.result); pending.delete(m.id); return; }
  if (m.method) events.push(m);
});
const { targetInfos } = await withTimeout(send('Target.getTargets'), 5000, 'getTargets');
const workers = targetInfos.filter(t => t.type === 'worker').slice(0, MAXW);
console.log(`sampling ${workers.length} of ${targetInfos.filter(t => t.type === 'worker').length} workers`);
for (const w of workers) {
  try {
    const { sessionId } = await withTimeout(send('Target.attachToTarget', { targetId: w.targetId, flatten: true }), 4000, 'attach');
    events.length = 0;
    await withTimeout(send('Debugger.enable', {}, sessionId), 3000, 'enable').catch(() => {});
    await withTimeout(send('Debugger.pause', {}, sessionId), 3000, 'pause').catch(() => {});
    await sleep(400);
    const pausedEv = events.filter(e => e.method === 'Debugger.paused').pop();
    if (pausedEv) {
      const frames = pausedEv.params.callFrames.slice(0, 4).map(f => {
        const url = (f.url || '').split('/').pop() || '(vm)';
        const fn = f.functionName || '(anon)';
        return `${fn}@${url}:${f.location.lineNumber}:${f.location.columnNumber}`;
      });
      console.log(`worker ${w.targetId.slice(0, 8)}: ${frames.join(' | ')}`);
      await withTimeout(send('Debugger.resume', {}, sessionId), 3000, 'resume').catch(() => {});
    } else {
      console.log(`worker ${w.targetId.slice(0, 8)}: (no pause — running)`);
    }
    await withTimeout(send('Target.detachFromTarget', { sessionId }), 3000, 'detach').catch(() => {});
  } catch (e) {
    console.log(`worker ${w.targetId.slice(0, 8)}: ${e.message}`);
  }
}
process.exit(0);
