const tabs = await (await fetch('http://127.0.0.1:9333/json/list')).json();
const t = tabs.find(x => x.id.startsWith(process.argv[2]));
const ws = new WebSocket(t.webSocketDebuggerUrl);
await new Promise(r => ws.addEventListener('open', r));
ws.addEventListener('message', ev => { const m = JSON.parse(ev.data); if (m.id === 1) { console.log(JSON.stringify(m.result.entries.map(e => e.url), null, 1), 'current', m.result.currentIndex); process.exit(0); } });
ws.send(JSON.stringify({ id: 1, method: 'Page.getNavigationHistory' }));
