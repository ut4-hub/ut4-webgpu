// Persistent CDP console logger: launches HEADED Brave with WebGPU/JSPI, navigates
// to the game, and streams ALL console/exception/network-failure output to stdout
// (redirect to a file). Stays alive so the user can interact live and we can read
// the log on demand. No screenshots (worker OffscreenCanvas -> black anyway).
// [2026-10-10] Resumed for UT4 WebGPU browser work; adds Memory64 flag.
import { spawn } from 'node:child_process';

const CHROME = process.argv[2];
const URL = process.argv[3];
const CDP_PORT = parseInt(process.argv[4] || '9333', 10);

const args = [
  `--remote-debugging-port=${CDP_PORT}`,
  `--enable-unsafe-webgpu`, `--enable-features=WebGPU,WebAssemblyMemory64`,
  `--enable-experimental-webassembly-jspi`, `--js-flags=--experimental-wasm-jspi`,
  `--ignore-gpu-blocklist`, `--no-first-run`, `--no-default-browser-check`,
  `--user-data-dir=/tmp/brave-live-cdp`, `--window-size=1400,900`, 'about:blank',
];
const chrome = spawn(CHROME, args, { stdio: ['ignore', 'pipe', 'pipe'], env: process.env });
const sleep = ms => new Promise(r => setTimeout(r, ms));
function ts(){ return new Date().toISOString().split('T')[1].replace('Z',''); }
function emit(l){ process.stdout.write(`[${ts()}] ${l}\n`); }

async function waitForCdp(){
  for (let i=0;i<60;i++){ try { const r=await fetch(`http://127.0.0.1:${CDP_PORT}/json/version`); if (r.ok) return; } catch(e){} await sleep(500); }
  throw new Error('CDP never came up');
}
async function main(){
  await waitForCdp();
  const nt = await (await fetch(`http://127.0.0.1:${CDP_PORT}/json/new?about:blank`,{method:'PUT'})).json();
  const ws = new WebSocket(nt.webSocketDebuggerUrl);
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
  emit(`--- navigating to ${URL} ---`);
  await send('Page.navigate',{url:URL});
  // Stay alive indefinitely so the user can interact; heartbeat marker every
  // 30s.
  let n=0; while(true){ await sleep(30000); emit(`--- alive ${++n} ---`); }
}
main().catch(e=>{ console.error('FATAL: '+e.stack); chrome.kill('SIGKILL'); process.exit(1); });
