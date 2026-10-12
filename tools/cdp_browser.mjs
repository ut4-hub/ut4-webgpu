const v = await (await fetch('http://127.0.0.1:9333/json/version')).json();
const ws = new WebSocket(v.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
const t = setTimeout(() => { console.log('timeout'); process.exit(2); }, 10000);
ws.addEventListener('message', ev => { const m = JSON.parse(ev.data); if (m.id === 1) { for (const ti of m.result.targetInfos.filter(x => x.type === 'page')) console.log(ti.targetId.slice(0,8), ti.attached, ti.url.slice(0,60)); process.exit(0);} });
ws.send(JSON.stringify({ id: 1, method: 'Target.getTargets' }));
