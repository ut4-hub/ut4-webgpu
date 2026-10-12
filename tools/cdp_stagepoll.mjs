// Poll main-thread-readable wasm probes every second; log when stalled.
// usage: node cdp_stagepoll.mjs <tab> > stagepoll.log
const PORT=9333, TAB=process.argv[2]||'3402B27F';
const tabs=await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const tab=tabs.find(t=>t.id.startsWith(TAB));
const ws=new WebSocket(tab.webSocketDebuggerUrl); let id=1; const pend=new Map();
const send=(m,p={})=>new Promise(r=>{const i=id++;pend.set(i,r);ws.send(JSON.stringify({id:i,method:m,params:p}));});
await new Promise(r=>ws.addEventListener('open',r));
ws.addEventListener('message',e=>{const m=JSON.parse(e.data); if(m.id&&pend.has(m.id)){pend.get(m.id)(m.result);pend.delete(m.id);}});
const expr=`(()=>{try{const M=Module;if(!M.__wndrRuntimeReady)return 'notready';return JSON.stringify({gf:M._wndr_game_frame(),rf:M._wndr_render_frame?M._wndr_render_frame():-1,gsite:M._wndr_probe_site?M._wndr_probe_site():-1,st:M._wndr_rhi_stage?M._wndr_rhi_stage():-1,bc:M._wndr_rhi_bind_calls?M._wndr_rhi_bind_calls():-1})}catch(e){return 'err '+e}})()`;
let last=null, same=0;
while(true){
  const r=await send('Runtime.evaluate',{expression:expr,returnByValue:true});
  const v=r?.result?.value; const t=new Date().toISOString().slice(11,23);
  let o=null; try{o=JSON.parse(v);}catch{}
  if(!o){ console.log(t,v); }
  else {
    const key=o.gf+'/'+o.bc;
    if(key===last) same++; else same=0;
    if(same===0 || same%5===0) console.log(t, JSON.stringify(o), same?`STALL ${same}s`:'');
    last=key;
  }
  await new Promise(r=>setTimeout(r,1000));
}
