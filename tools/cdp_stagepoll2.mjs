// Every 5s: game frame, bind calls, and per-RHI-stage ms deltas (top stages).
const PORT=9333, TAB=process.argv[2]||'3402B27F';
const ST=[0,10,12,20,21,22,23,24,25,26,27,28,30,31,32,40,41,42,50,60,61];
const tabs=await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const tab=tabs.find(t=>t.id.startsWith(TAB));
const ws=new WebSocket(tab.webSocketDebuggerUrl); let id=1; const pend=new Map();
const send=(m,p={})=>new Promise(r=>{const i=id++;pend.set(i,r);ws.send(JSON.stringify({id:i,method:m,params:p}));});
await new Promise(r=>ws.addEventListener('open',r));
ws.addEventListener('message',e=>{const m=JSON.parse(e.data); if(m.id&&pend.has(m.id)){pend.get(m.id)(m.result);pend.delete(m.id);}});
const expr=`(()=>{try{const M=Module;if(!M.__wndrRuntimeReady||!M._wndr_rhi_stage_ms)return 'notready';return JSON.stringify({gf:M._wndr_game_frame(),bc:M._wndr_rhi_bind_calls(),st:M._wndr_rhi_stage(),ms:${JSON.stringify(ST)}.map(i=>M._wndr_rhi_stage_ms(i))})}catch(e){return 'err '+e}})()`;
let prev=null;
while(true){
  const r=await send('Runtime.evaluate',{expression:expr,returnByValue:true});
  const v=r?.result?.value; const t=new Date().toISOString().slice(11,19);
  let o=null; try{o=JSON.parse(v);}catch{}
  if(!o) console.log(t,v);
  else {
    if(prev){
      const d=o.ms.map((x,i)=>[ST[i],x-prev.ms[i]]).filter(x=>x[1]>1).sort((a,b)=>b[1]-a[1]).slice(0,6).map(([s,m])=>`s${s}:${m.toFixed(0)}`).join(' ');
      console.log(t,`gf=${o.gf} (+${o.gf-prev.gf}) binds+${o.bc-prev.bc} cur=${o.st} | ${d}`);
    }
    prev=o;
  }
  await new Promise(r=>setTimeout(r,5000));
}
