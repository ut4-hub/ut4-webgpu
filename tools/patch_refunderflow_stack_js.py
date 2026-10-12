#!/usr/bin/env python3
# patch_refunderflow_stack_js.py <js-file>
# UBRACE hunt: attach the JS/wasm call stack to the first WNDR REFUNDERFLOW lines.
# printf -> fd_write -> out() runs synchronously on the over-releasing thread, so
# new Error().stack names the exact FRHIResource::Release() caller. No wasm rebuild.
# Idempotent.
import sys

MARK = 'wndrRufProbe'
OLD = "  if (ENVIRONMENT_IS_PTHREAD) return proxyToMainThread(59, 0, 1, fd, iov, iovcnt, pnum);"
NEW = """  if (ENVIRONMENT_IS_PTHREAD) { wndrRufProbe(iov, iovcnt); return proxyToMainThread(59, 0, 1, fd, iov, iovcnt, pnum); }"""
HELPER = """
function wndrRufProbe(iov, iovcnt) {
  try {
    if ((wndrRufProbe.n | 0) >= 24) return;
    var H = (growMemViews(), HEAPU64), t = "", hit = false;
    for (var k = 0; k < Math.min(Number(iovcnt), 4) && !hit; k++) {
      var i = Number(iov) / 8 + 2 * k, p = Number(H[i]), l = Number(H[i + 1]);
      if (l < 20) continue;
      t = UTF8ArrayToString(HEAPU8, p, Math.min(l, 200));
      hit = t.indexOf("WNDR REFUNDERFLOW") >= 0;
    }
    if (!hit) return;
    wndrRufProbe.n = (wndrRufProbe.n | 0) + 1;
    var lim = Error.stackTraceLimit; Error.stackTraceLimit = 40;
    var st = new Error().stack; Error.stackTraceLimit = lim;
    console.log("WNDR RUF-STACK " + t + "\\n" + st);
  } catch (e) {}
}
"""

def main(path):
    s = open(path).read()
    if MARK in s:
        print('already patched'); return
    assert s.count(OLD) == 1, s.count(OLD)
    s = s.replace(OLD, NEW)
    s = s.replace('function _fd_write(fd, iov, iovcnt, pnum) {', HELPER + 'function _fd_write(fd, iov, iovcnt, pnum) {', 1)
    open(path, 'w').write(s)
    print('patched OK')

main(sys.argv[1])
