#!/usr/bin/env python3
# patch_async_stdout_js.py <js-file>  (run AFTER patch_refunderflow_stack_js.py)
# PERF: printf/fflush on a pthread proxied fd_write SYNCHRONOUSLY to the main thread
# (~13% of the game thread's time). stdout/stderr from pthreads now post complete
# lines to the main thread fire-and-forget; the main thread console.logs them.
import sys
MARK = 'wndrAsyncWrite'
OLD = "  if (ENVIRONMENT_IS_PTHREAD) { wndrRufProbe(iov, iovcnt); return proxyToMainThread(59, 0, 1, fd, iov, iovcnt, pnum); }"
NEW = "  if (ENVIRONMENT_IS_PTHREAD) { wndrRufProbe(iov, iovcnt); if (fd == 1 || fd == 2) return wndrAsyncWrite(fd, iov, iovcnt, pnum); return proxyToMainThread(59, 0, 1, fd, iov, iovcnt, pnum); }"
HELPER = """
function wndrAsyncWrite(fd, iov, iovcnt, pnum) {
  var H = (growMemViews(), HEAPU64), n = 0, t = "", base = Number(iov) / 8;
  for (var k = 0; k < Number(iovcnt); k++) { var p = Number(H[base + 2 * k]), l = Number(H[base + 2 * k + 1]); if (l) { t += UTF8ArrayToString(HEAPU8, p, l); n += l; } }
  var b = wndrAsyncWrite.buf || (wndrAsyncWrite.buf = {});
  var s = (b[fd] || "") + t, i = s.lastIndexOf("\\n");
  if (i >= 0) { postMessage({ cmd: "wndrlog", fd: fd, text: s.slice(0, i) }); s = s.slice(i + 1); }
  b[fd] = s;
  H[Number(pnum) / 8] = BigInt(n);
  return 0;
}
"""
OLD2 = """      switch (cmd) {
       case 4:
        checkMailbox();"""
NEW2 = """      if (cmd === "wndrlog") { var _ls = d.text.split("\\n"); for (var _li = 0; _li < _ls.length; _li++) { if (d.fd == 2) (Module.printErr || err)(_ls[_li]); else (Module.print || out)(_ls[_li]); } return; }
      switch (cmd) {
       case 4:
        checkMailbox();"""
def main(path):
    s = open(path).read()
    if MARK in s: print('already patched'); return
    assert s.count(OLD) == 1, ('fd_write', s.count(OLD))
    assert s.count(OLD2) == 1, ('dispatch', s.count(OLD2))
    s = s.replace(OLD, NEW).replace(OLD2, NEW2)
    s = s.replace('function _fd_write(fd, iov, iovcnt, pnum) {', HELPER + 'function _fd_write(fd, iov, iovcnt, pnum) {', 1)
    open(path, 'w').write(s); print('patched OK')
main(sys.argv[1])
