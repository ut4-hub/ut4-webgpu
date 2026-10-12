// Profile page main + all workers for N seconds; summarize per-thread self time & wait callers.
import fs from 'node:fs';
const PORT=9333, TAB=process.argv[2]||'3402B27F', SECS=+(process.argv[3]||30);
const tabs=await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const tab=tabs.find(t=>t.id.startsWith(TAB));
const ws=new WebSocket(tab.webSocketDebuggerUrl); let id=1; const pend=new Map();
const send=(m,p={},sessionId)=>new Promise((r,j)=>{const i=id++;pend.set(i,{r,j});ws.send(JSON.stringify({id:i,method:m,params:p,sessionId}));setTimeout(()=>{if(pend.has(i)){pend.delete(i);j(new Error('timeout '+m));}},20000);});
await new Promise(r=>ws.addEventListener('open',r));
ws.addEventListener('message',e=>{const m=JSON.parse(e.data); if(m.id&&pend.has(m.id)){const p=pend.get(m.id);pend.delete(m.id); m.error?p.j(new Error(JSON.stringify(m.error))):p.r(m.result);}});
const {targetInfos}=await send('Target.getTargets');
const sess=[{name:'MAIN',sid:undefined}];
for (const w of targetInfos.filter(t=>t.type==='worker')) { try{ const {sessionId}=await send('Target.attachToTarget',{targetId:w.targetId,flatten:true}); sess.push({name:w.targetId.slice(0,8),sid:sessionId}); }catch(e){console.log('attach fail',w.targetId.slice(0,8),e.message);} }
for (const s of sess){ try{ await send('Profiler.enable',{},s.sid); await send('Profiler.setSamplingInterval',{interval:2000},s.sid); await send('Profiler.start',{},s.sid); s.ok=true;}catch(e){console.log('start fail',s.name,e.message);} }
console.log('profiling',sess.filter(s=>s.ok).length,'threads for',SECS,'s');
await new Promise(r=>setTimeout(r,SECS*1000));
const out={};
for (const s of sess){ if(!s.ok) continue; try{ const {profile}=await send('Profiler.stop',{},s.sid); out[s.name]=profile; }catch(e){console.log('stop fail',s.name,e.message);} }
fs.writeFileSync('prof-all.json',JSON.stringify(out));
for (const [name,p] of Object.entries(out)){
  const byId=new Map(p.nodes.map(n=>[n.id,n])); const parent=new Map();
  for(const n of p.nodes) for(const c of (n.children||[])) parent.set(c,n.id);
  const cnt=new Map(); for(const s of p.samples) cnt.set(s,(cnt.get(s)||0)+1);
  const total=p.samples.length; const self=new Map(); const stacks=new Map();
  for(const [nid,c] of cnt){ const n=byId.get(nid); const fn=n.callFrame.functionName||'('+(n.callFrame.url?'anon':'native')+')'; self.set(fn,(self.get(fn)||0)+c);
    if(!['(idle)','(program)','(garbage collector)'].includes(fn)){ let chain=[]; let x=nid; while(x&&chain.length<12){const nn=byId.get(x); chain.push(nn.callFrame.functionName||'?'); x=parent.get(x);} const k=chain.join(' < '); stacks.set(k,(stacks.get(k)||0)+c);} }
  const top=[...self].sort((a,b)=>b[1]-a[1]).slice(0,6).map(([f,c])=>`${f}:${(100*c/total).toFixed(0)}%`).join('  ');
  console.log(`\n== ${name} samples=${total}  ${top}`);
  for(const [k,c] of [...stacks].sort((a,b)=>b[1]-a[1]).slice(0,3)) console.log(`   ${(100*c/total).toFixed(0)}% ${k}`);
}
process.exit(0);
