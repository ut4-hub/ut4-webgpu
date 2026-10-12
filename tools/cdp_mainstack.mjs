const PORT=9333, TAB=process.argv[2]||'3402B27F';
const tabs=await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const tab=tabs.find(t=>t.id.startsWith(TAB));
const ws=new WebSocket(tab.webSocketDebuggerUrl); let id=1; const pend=new Map(); const ev=[];
const send=(m,p={})=>new Promise(r=>{const i=id++;pend.set(i,r);ws.send(JSON.stringify({id:i,method:m,params:p}));});
await new Promise(r=>ws.addEventListener('open',r));
ws.addEventListener('message',e=>{const m=JSON.parse(e.data); if(m.id&&pend.has(m.id)){pend.get(m.id)(m.result);pend.delete(m.id);} else if(m.method) ev.push(m);});
await send('Debugger.enable');
for (let k=0;k<5;k++){
  ev.length=0; await send('Debugger.pause'); await new Promise(r=>setTimeout(r,800));
  const p=ev.filter(e=>e.method==='Debugger.paused').pop();
  if(p){ console.log('--- sample',k); for(const f of p.params.callFrames.slice(0,18)) console.log('  ',f.functionName||'(anon)', (f.url||'').split('/').pop()+':'+f.location.lineNumber); await send('Debugger.resume'); }
  else console.log('--- sample',k,'no pause (main idle in event loop)');
  await new Promise(r=>setTimeout(r,1500));
}
process.exit(0);
