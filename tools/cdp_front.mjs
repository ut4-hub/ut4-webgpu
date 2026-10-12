// One-shot: check page visibility and bring the game tab to front.
// usage: node cdp_front.mjs [tabID-prefix] [cdpPort]
const PORT = parseInt(process.argv[3] || '9333', 10);
const PREFIX = process.argv[2] || '';
const tabs = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const tab = tabs.find(t => t.id.startsWith(PREFIX)) || tabs.find(t => t.url.includes('127.0.0.1:8091')) || tabs[0];
console.log(`tab=${tab.id} url=${tab.url}`);
const ws = new WebSocket(tab.webSocketDebuggerUrl);
let id = 1; const pending = new Map();
const send = (m, p = {}) => new Promise(res => { const i = id++; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: m, params: p })); });
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
ws.addEventListener('message', ev => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); }
});
const vis = await send('Runtime.evaluate', { expression: 'document.visibilityState + " focus=" + document.hasFocus()', returnByValue: true });
console.log('visibility:', vis.result?.value);
await send('Page.bringToFront');
const vis2 = await send('Runtime.evaluate', { expression: 'document.visibilityState + " focus=" + document.hasFocus()', returnByValue: true });
console.log('after bringToFront:', vis2.result?.value);
process.exit(0);
