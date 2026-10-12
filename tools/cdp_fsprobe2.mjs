// One-shot: check whether an FS bridge exists; look for emscripten FS export
// via Module.FS or FS in global scope, then stat the AssetRegistry paths.
// usage: node cdp_fsprobe2.mjs [cdpPort]
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
    try { out.FStypeof = typeof FS; } catch(e) { out.FStypeof = 'throw:' + e.message; }
    try { out.ModuleFS = typeof Module.FS; } catch(e) { out.ModuleFS = 'throw'; }
    try { out.ccall = typeof Module.ccall; } catch(e) {}
    try { out._.stat = typeof Module._stat; } catch(e) {}
    // try ccall stat if available
    if (typeof Module !== 'undefined' && typeof Module.ccall === 'function' && typeof Module._stat === 'function') {
      try {
        const buf = Module._malloc(512);
        const name = Module._malloc(128);
        Module.stringToUTF8('/UnrealTournament/AssetRegistry.bin', name, 128);
        const rc = Module.ccall('stat', 'number', ['string','pointer'], ['/UnrealTournament/AssetRegistry.bin', buf]);
        out.statRC = rc;
        out.st_size = Module.getValue(buf + 88, 'i64'); // struct stat layout guess
        Module._free(buf); Module._free(name);
      } catch (e) { out.statErr = String(e); }
    }
    return JSON.stringify(out);
  })()`,
  returnByValue: true,
});
console.log(r.result?.value ?? JSON.stringify(r.result));
process.exit(0);
