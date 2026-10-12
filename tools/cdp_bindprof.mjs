// node cdp_bindprof.mjs <tab> [seconds]: per-stage bind-walk ms + game frames over a window
const [PREFIX, SECS] = [process.argv[2], parseFloat(process.argv[3] || '10')];
const v = await (await fetch('http://127.0.0.1:9333/json/version')).json();
const ws = new WebSocket(v.webSocketDebuggerUrl);
await new Promise((res, rej) => { ws.addEventListener('open', res); ws.addEventListener('error', rej); });
let id = 1; const pending = new Map();
const send = (m, p = {}, sessionId) => new Promise(res => { const i = id++; pending.set(i, res); ws.send(JSON.stringify({ id: i, method: m, params: p, sessionId })); });
ws.addEventListener('message', ev => { const m = JSON.parse(ev.data); if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); } });
const tg = (await send('Target.getTargets')).result.targetInfos.find(t => t.targetId.startsWith(PREFIX));
const s = (await send('Target.attachToTarget', { targetId: tg.targetId, flatten: true })).result.sessionId;
const expr = `(() => { const o = {t: performance.now(), gf: Number(Module._wndr_game_frame()), rf: Number(Module._wndr_render_frame()), calls: Number(Module._wndr_rhi_bind_calls()), st: []}; for (let i = 0; i < 70; i++) o.st.push(Number(Module._wndr_rhi_stage_ms(i))); return JSON.stringify(o); })()`;
const a = JSON.parse((await send('Runtime.evaluate', { expression: expr, returnByValue: true }, s)).result.result.value);
await new Promise(r => setTimeout(r, SECS * 1000));
const b = JSON.parse((await send('Runtime.evaluate', { expression: expr, returnByValue: true }, s)).result.result.value);
const dt = (b.t - a.t) / 1000, gf = b.gf - a.gf, rf = b.rf - a.rf;
console.log(`window ${dt.toFixed(1)}s  game frames ${gf} (${(gf/dt).toFixed(1)} fps)  render frames ${rf} (${(rf/dt).toFixed(1)} fps)  binds ${b.calls - a.calls} (${((b.calls-a.calls)/Math.max(rf,1)).toFixed(0)}/frame)`);
let tot = 0; const rows = [];
for (let i = 0; i < 70; i++) { const d = b.st[i] - a.st[i]; if (d > 0.5) { rows.push([i, d]); tot += d; } }
rows.sort((x, y) => y[1] - x[1]);
console.log(`bind-walk total ${tot.toFixed(0)} ms = ${(tot / dt / 10).toFixed(1)}% of wall, ${(tot / Math.max(rf,1)).toFixed(1)} ms/frame`);
for (const [i, d] of rows.slice(0, 12)) console.log(`  stage ${i}: ${d.toFixed(0)} ms (${(d / Math.max(rf,1)).toFixed(2)} ms/frame)`);
process.exit(0);
