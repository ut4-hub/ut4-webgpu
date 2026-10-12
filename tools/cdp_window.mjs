// One-shot: inspect/restore the browser window for the game tab.
// usage: node cdp_window.mjs [tabID-prefix] [cdpPort]
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
const win = await send('Browser.getWindowForTarget');
console.log('windowForTarget:', JSON.stringify(win));
if (win?.windowId) {
  const bounds = await send('Browser.getWindowBounds', { windowId: win.windowId });
  console.log('bounds:', JSON.stringify(bounds));
  if (bounds?.bounds?.windowState === 'minimized' || bounds?.bounds?.windowState === 'fullscreen') {
    const r = await send('Browser.setWindowBounds', { windowId: win.windowId, bounds: { windowState: 'normal', left: 50, top: 50, width: 1400, height: 900 } });
    console.log('restore:', JSON.stringify(r));
  }
}
await send('Page.bringToFront');
process.exit(0);
