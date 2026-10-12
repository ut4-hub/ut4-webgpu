#!/usr/bin/env python3
"""Serialize the 7 big ContentBootstrap package fetches via a global promise chain.

Each ContentBootstrap_{1..7}.js kicks off `fetchRemotePackage` immediately at
script-eval time. Seven parallel ~1GB streams each hold: reader chunks (~1GB) +
Blob (~1GB) + arrayBuffer (~1GB) => ~14GB transient on a 32GB Mac that also
reserves a 16GB wasm heap (virtual, but backing pages count once touched) =>
"Array buffer allocation failed" => hang-guard drops containers => missing
IoStore containers => GDefaultMaterials assert.

Fix: route each package's fetch+process through a global FIFO
(window.__wndrPkgChain). Only one package is in its memory-heavy phase at a
time. Order is preserved because each script's IIFE awaits its predecessor.
Idempotent-ish: safe to re-run (replaces the same marker block).
"""
import re, sys

for n in range(1, 8):
    p = f"/Volumes/ExFAT 1TB/ut4-webgpu/ut-serve/ContentBootstrap_{n}.js"
    s = open(p).read()

    # 1) Serialize the fetch start: instead of starting fetchRemotePackage
    #    immediately, chain it behind the global queue.
    old_start = """      if (!fetched) {
        // Note that we don't use await here because we want to execute the
        // the rest of this function immediately.
        fetchPromise = fetchRemotePackage(REMOTE_PACKAGE_NAME, REMOTE_PACKAGE_SIZE);
      }"""
    new_start = """      if (!fetched) {
        // wndr-serial: one big package in memory at a time (7 parallel ~1GB
        // fetch+stitch bursts OOM the tab). Chain behind a global FIFO.
        if (!window.__wndrPkgChain) window.__wndrPkgChain = Promise.resolve();
        fetchPromise = window.__wndrPkgChain.then(function(){ return fetchRemotePackage(REMOTE_PACKAGE_NAME, REMOTE_PACKAGE_SIZE); });
      }"""
    if old_start not in s:
        print(f"{p}: fetch-start anchor not found", file=sys.stderr); continue
    s = s.replace(old_start, new_start)

    # 2) Extend the chain to cover processPackageData (the other memory-heavy
    #    phase: FS_createDataFile copies into MEMFS) so stitch+mount don't
    #    overlap either. Wrap the final await.
    old_tail = """      await processPackageData(fetched);

    }"""
    new_tail = """      // wndr-serial: run the MEMFS ingestion under the same global lock, then
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
    if old_tail not in s:
        print(f"{p}: tail anchor not found", file=sys.stderr); continue
    s = s.replace(old_tail, new_tail)

    open(p, "w").write(s)
    print(f"serialized OK: ContentBootstrap_{n}.js")
