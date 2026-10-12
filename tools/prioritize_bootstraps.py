#!/usr/bin/env python3
"""Priority-order the serialized bootstrap fetches.

Problem: the serialized chain runs packages in script-tag order (1..7). By the
time utcontent6/7 (the WEAPON containers: UTWeap_* classes, 3.6k+4.3k entries)
get their turn, ~5GB of MEMFS data is resident and their ~1GB transient stitch
allocs fail -> hang-guard drops them -> no gun/HUD/fire in game.

Fix: a global token bucket with an explicit priority order. Each bootstrap N
acquires the global lock only when the ordered queue says it's its turn.
Implementation: window.__wndrPkgOrder = [array of expected package labels];
window.__wndrPkgTurn tracks progress. Each file waits its turn, runs, then
advances the pointer. Simple, deadlock-free (strict order), memory-flat.
"""
import sys

ORDER = [6, 7, 1, 5, 4, 2, 3]  # weapons first, then ascending size/criticality

WAIT_HELPER = """
      // wndr-priority: strict global order for the big package fetches.
      // Weapons containers (6,7) go first so their ~1GB transient stitch
      // allocations happen while MEMFS is still small; later, larger
      // allocations are the ones that can fail and be retried cheaply.
      if (!window.__wndrPkgOrder) {
        window.__wndrPkgOrder = %ORDER%;
        window.__wndrPkgTurn = 0;
      }
      async function __wndrWaitTurn(idx) {
        while (true) {
          if (window.__wndrPkgOrder[window.__wndrPkgTurn] === idx) return;
          await new Promise(function(r){ setTimeout(r, 50); });
        }
      }
      async function __wndrNextTurn() {
        window.__wndrPkgTurn = (window.__wndrPkgTurn + 1) %% window.__wndrPkgOrder.length;
      }
""".replace("%ORDER%", "[" + ",".join(str(x) for x in ORDER) + "]")

for n in range(1, 8):
    p = f"/Volumes/ExFAT 1TB/ut4-webgpu/ut-serve/ContentBootstrap_{n}.js"
    s = open(p).read()

    if "__wndrPkgOrder" in s:
        print(f"already priority-patched: {n}")
        continue

    # Replace the previous wndr-serial chain (fetch + process under global chain)
    old_start = """      if (!fetched) {
        // wndr-serial: one big package in memory at a time (7 parallel ~1GB
        // fetch+stitch bursts OOM the tab). Chain behind a global FIFO.
        if (!window.__wndrPkgChain) window.__wndrPkgChain = Promise.resolve();
        fetchPromise = window.__wndrPkgChain.then(function(){ return fetchRemotePackage(REMOTE_PACKAGE_NAME, REMOTE_PACKAGE_SIZE); });
      }"""
    new_start = WAIT_HELPER + """
      if (!fetched) {
        // wndr-priority: fetch in strict global order (see __wndrWaitTurn).
        fetchPromise = (async function(){
          await __wndrWaitTurn(%N%);
          try {
            return await fetchRemotePackage(REMOTE_PACKAGE_NAME, REMOTE_PACKAGE_SIZE);
          } finally {
            await __wndrNextTurn();
          }
        })();
      }"""
    if old_start not in s:
        print(f"{p}: serial fetch-start anchor not found", file=sys.stderr); sys.exit(1)
    s = s.replace(old_start, new_start)

    # Remove the wndr-serial tail (chain over processPackageData); keep plain await.
    old_tail = """      // wndr-serial: run the MEMFS ingestion under the same global lock, then
      // release it for the next package.
      var __prev = window.__wndrPkgChain;
      window.__wndrPkgChain = __prev.then(function(){ return processPackageData(fetched); });
      try {
        await window.__wndrPkgChain;
      } catch (e) {
        // hang-guard already reported; keep the chain alive for later packages
        if (!window.__wndrPkgChain || window.__wndrPkgChain === Promise.resolve()) {}
      }

    }"""
    new_tail = """      await processPackageData(fetched);

    }"""
    if old_tail not in s:
        print(f"{p}: serial tail anchor not found", file=sys.stderr); sys.exit(1)
    s = s.replace(old_tail, new_tail)

    s = s.replace("await __wndrNextTurn();", "await __wndrNextTurn(%N%);".replace("%N%", str(n)))
    s = s.replace("""      async function __wndrNextTurn() {
        window.__wndrPkgTurn = (window.__wndrPkgTurn + 1) % window.__wndrPkgOrder.length;
      }""", """      async function __wndrNextTurn(me) {
        window.__wndrPkgTurn = (window.__wndrPkgTurn + 1) % window.__wndrPkgOrder.length;
      }""")
    open(p, "w").write(s)
    print(f"priority OK: ContentBootstrap_{n}.js")
