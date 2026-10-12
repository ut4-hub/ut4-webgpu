// Poke the page main thread every N ms with a trivial evaluate.
const PORT=9333, TAB=process.argv[2]||'3402B27F', MS=+(process.argv[3]||50);
const tabs=await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const tab=tabs.find(t=>t.id.startsWith(TAB));
const ws=new WebSocket(tab.webSocketDebuggerUrl); let id=1;
await new Promise(r=>ws.addEventListener('open',r));
setInterval(()=>ws.send(JSON.stringify({id:id++,method:'Runtime.evaluate',params:{expression:'1'}})),MS);
