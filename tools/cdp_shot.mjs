// One-shot: capture compositor screenshot of the game tab.
// usage: node cdp_shot.mjs <outfile.png> [tabID-prefix] [cdpPort]
import { writeFileSync } from 'node:fs';
const OUT = process.argv[2];
const PORT = parseInt(process.argv[4] || '9333', 10);
const PREFIX = process.argv[3] || '';
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
await send('Page.enable');
await send('Page.bringToFront');
const shot = await send('Page.captureScreenshot', { format: 'png' });
if (!shot?.data) { console.error('no screenshot data', JSON.stringify(shot).slice(0, 200)); process.exit(1); }
writeFileSync(OUT, Buffer.from(shot.data, 'base64'));
console.log(`wrote ${OUT} (${Math.round(shot.data.length * 3 / 4 / 1024)} KB) from tab ${tab.id}`);
process.exit(0);
