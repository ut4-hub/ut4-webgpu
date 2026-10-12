// One-shot: report visibility/focus state of the game tab.
// usage: node cdp_vis.mjs [tabID-prefix] [cdpPort]
const PORT = parseInt(process.argv[3] || '9333', 10);
const PREFIX = process.argv[2] || '';
const tabs = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const tab = tabs.find(t => t.id.startsWith(PREFIX)) || tabs.find(t => t.url.includes('127.0.0.1:8091')) || tabs[0];
const ws = new WebSocket(tab.webSocketDebuggerUrl);
let id = 1; const pending = new Map();
const send = (m, p = {}) => new Promise(res => { const i = id++; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: m, params: p })); });
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
ws.addEventListener('message', ev => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); }
});
const r = await send('Runtime.evaluate', {
  expression: 'JSON.stringify({vis: document.visibilityState, focus: document.hasFocus(), hidden: document.hidden})',
  returnByValue: true,
});
console.log('game tab:', r.result?.value);
process.exit(0);
