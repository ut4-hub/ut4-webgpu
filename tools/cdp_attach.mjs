// Attach to an EXISTING tab (by id prefix arg) and stream console/exceptions.
// usage: node cdp_attach.mjs <tabID> [cdpPort]
import { setTimeout as sleep } from 'node:timers/promises';

const TAB = process.argv[2];
const PORT = parseInt(process.argv[3] || '9333', 10);
function ts(){ return new Date().toISOString().split('T')[1].replace('Z',''); }
function emit(l){ process.stdout.write(`[${ts()}] ${l}\n`); }

const tabs = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
const tab = tabs.find(t => t.id.startsWith(TAB));
if (!tab) { console.error('tab not found'); process.exit(1); }
const ws = new WebSocket(tab.webSocketDebuggerUrl);
let id=1; const pending=new Map(); const reqUrls=new Map();
const send=(m,p={})=>new Promise(res=>{const i=id++;pending.set(i,res);ws.send(JSON.stringify({id:i,method:m,params:p}));});
await new Promise((res,rej)=>{ws.addEventListener('open',res);ws.addEventListener('error',rej);});
ws.addEventListener('message',ev=>{
  const m=JSON.parse(ev.data);
  if (m.id&&pending.has(m.id)){pending.get(m.id)(m.result);pending.delete(m.id);return;}
  switch(m.method){
    case 'Runtime.consoleAPICalled':{ const t=(m.params.args||[]).map(a=>a.value!==undefined?a.value:(a.description||'')).join(' '); emit(`[c.${m.params.type}] ${t}`); break; }
    case 'Runtime.exceptionThrown': emit(`[EXCEPTION] ${JSON.stringify(m.params.exceptionDetails).slice(0,600)}`); break;
    case 'Log.entryAdded':{ const e=m.params.entry; emit(`[log.${e.level}] ${e.text}${e.url?' url='+e.url:''}`); break; }
    case 'Network.requestWillBeSent': reqUrls.set(m.params.requestId, m.params.request.url); break;
    case 'Network.loadingFailed':{ const u=reqUrls.get(m.params.requestId)||'?'; emit(`[net.FAIL] ${u} :: ${m.params.errorText}`); break; }
  }
});
await send('Runtime.enable'); await send('Log.enable'); await send('Network.enable'); await send('Page.enable');
emit(`--- attached to ${tab.id} ${tab.url} ---`);
let n=0; while(true){ await sleep(30000); emit(`--- alive ${++n} ---`); }
