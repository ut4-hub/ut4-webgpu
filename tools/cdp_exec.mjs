// One-shot: execute a UE console command in the game tab by dispatching the
// FSimplyStreamApplication::Exec path via key events is not available, so try
// the known wasm exports; if none, report what's reachable.
// usage: node cdp_exec.mjs "r.MobileEyeAdaptation 0" [tabID-prefix] [cdpPort]
import { readFileSync } from 'node:fs';
const CMD = process.argv[2];
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
await send('Runtime.enable');
// List all Module._wndr_* exports reachable, to see if there's an exec bridge
const r = await send('Runtime.evaluate', {
  expression: `Object.keys(typeof Module !== 'undefined' ? Module : {}).filter(k => k.startsWith('_wndr')).join(' ')`,
  returnByValue: true,
});
console.log('wndr exports:', r.result?.value);
// Check for a generic exec
const hasExec = await send('Runtime.evaluate', {
  expression: `typeof Module !== 'undefined' ? (typeof Module._wndr_exec + ' ' + typeof Module._wndr_console + ' ' + typeof Module._wndr_cvar) : 'no Module'`,
  returnByValue: true,
});
console.log('exec-ish exports:', hasExec.result?.value);
process.exit(0);
