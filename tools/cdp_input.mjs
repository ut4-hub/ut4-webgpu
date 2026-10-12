// One-shot: send synthetic input to the game tab (click / key).
// usage: node cdp_input.mjs <click|x|w|space|...> [tabID-prefix] [cdpPort]
const ACTION = process.argv[2] || 'click';
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
await send('Runtime.enable');
await send('Emulation.focusEmulatedPage').catch(() => {});
const CX = 700, CY = 400;
if (ACTION === 'click') {
  await send('Input.dispatchMouseEvent', { type: 'mousePressed', x: CX, y: CY, button: 'left', buttons: 1, clickCount: 1, pointerType: 'mouse' });
  await send('Input.dispatchMouseEvent', { type: 'mouseReleased', x: CX, y: CY, button: 'left', buttons: 0, clickCount: 1, pointerType: 'mouse' });
  console.log('sent left click at', CX, CY);
} else {
  const key = ACTION === 'space' ? 'Space' : ACTION.length === 1 ? 'Key' + ACTION.toUpperCase() : ACTION;
  const code = key.startsWith('Key') ? key : ACTION;
  await send('Input.dispatchKeyEvent', { type: 'keyDown', key: ACTION === 'space' ? ' ' : ACTION, code, windowsVirtualKeyCode: ACTION === 'space' ? 32 : ACTION.toUpperCase().charCodeAt(0), nativeVirtualKeyCode: ACTION === 'space' ? 32 : ACTION.toUpperCase().charCodeAt(0) });
  await new Promise(r => setTimeout(r, 120));
  await send('Input.dispatchKeyEvent', { type: 'keyUp', key: ACTION === 'space' ? ' ' : ACTION, code, windowsVirtualKeyCode: ACTION === 'space' ? 32 : ACTION.toUpperCase().charCodeAt(0), nativeVirtualKeyCode: ACTION === 'space' ? 32 : ACTION.toUpperCase().charCodeAt(0) });
  console.log('sent key', ACTION);
}
const vis = await send('Runtime.evaluate', { expression: 'document.hasFocus()', returnByValue: true });
console.log('hasFocus:', vis.result?.value);
process.exit(0);
