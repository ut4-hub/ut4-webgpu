#!/usr/bin/env python3
# patch_worker_errstack_js.py <js-file>
# CRASH-hunt: pthread errors that escape outside handleMessage (async callbacks,
# rejected promises) reach the main thread as a bare "Pthread ... sent an error!"
# with no stack. Log the worker-side stack (proxied to main via err()) first.
# Idempotent.
import sys

MARK = 'WNDR WORKER-ERR'
OLD = '''  self.onunhandledrejection = e => {
    throw e.reason || e;
  };'''
NEW = '''  self.onunhandledrejection = e => {
    try { var r = e.reason || e; err('WNDR WORKER-ERR (rejection) ' + (r && r.stack ? r.stack : r)); } catch (_) {}
    throw e.reason || e;
  };
  self.addEventListener('error', ev => {
    try { err('WNDR WORKER-ERR ' + (ev.error && ev.error.stack ? ev.error.stack : (ev.message + ' @' + ev.filename + ':' + ev.lineno))); } catch (_) {}
  });'''

def main(path):
    s = open(path).read()
    if MARK in s:
        print('already patched'); return
    assert s.count(OLD) == 1, s.count(OLD)
    open(path, 'w').write(s.replace(OLD, NEW))
    print('patched OK')

main(sys.argv[1])
