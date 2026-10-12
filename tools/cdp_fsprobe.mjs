// One-shot: probe MEMFS visibility of AssetRegistry.bin from the game page.
// usage: node cdp_fsprobe.mjs [cdpPort]
const PORT = parseInt(process.argv[2] || '9333', 10);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const tabs = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const tab = tabs.find(t => t.url.includes('127.0.0.1:8091'));
if (!tab) { console.error('no game tab'); process.exit(1); }
const ws = new WebSocket(tab.webSocketDebuggerUrl);
let id = 1; const pending = new Map(); const events = [];
const send = (m, p = {}) => new Promise(res => { const i = id++; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: m, params: p })); });
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
ws.addEventListener('message', ev => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); return; }
  if (m.method) events.push(m);
});
await send('Runtime.enable');
const evalr = await send('Runtime.evaluate', {
  expression: `(function(){
    if (typeof Module === 'undefined') return 'no Module';
    return JSON.stringify({
      iasCount: (typeof Module._wndr_ias_count_fetch === 'function') ? Module._wndr_ias_count_fetch() : -1,
      paksReady: (typeof Module._wndr_ias_path_paks_ready === 'function') ? Module._wndr_ias_path_paks_ready() : -1,
      gameFrame: (typeof Module._wndr_game_frame === 'function') ? Module._wndr_game_frame() : -1,
    });
  })()`,
  returnByValue: true,
});
console.log('result:', JSON.stringify(evalr.result));
process.exit(0);
