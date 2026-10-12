// One-shot: stat MEMFS paths via global FS (exists on the main thread).
// usage: node cdp_fsprobe3.mjs [cdpPort]
const PORT = parseInt(process.argv[2] || '9333', 10);
const tabs = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const tab = tabs.find(t => t.url.includes('127.0.0.1:8091'));
const ws = new WebSocket(tab.webSocketDebuggerUrl);
let id = 1; const pending = new Map();
const send = (m, p = {}) => new Promise(res => { const i = id++; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: m, params: p })); });
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
ws.addEventListener('message', ev => { const m = JSON.parse(ev.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); } });
const r = await send('Runtime.evaluate', {
  expression: `(function(){
    const out = {};
    const paths = [
      '/UnrealTournament/AssetRegistry.bin',
      '/UnrealTournament/UnrealTournament.uproject',
      '/Engine/GlobalShaderCache-None.bin',
    ];
    for (const p of paths) {
      try {
        const s = FS.stat(p);
        out[p] = s.size;
      } catch (e) {
        out[p] = 'MISSING';
      }
    }
    out.cwd = FS.cwd();
    return JSON.stringify(out);
  })()`,
  returnByValue: true,
});
console.log(r.result?.value ?? JSON.stringify(r.result));
process.exit(0);
