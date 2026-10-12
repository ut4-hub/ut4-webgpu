#!/usr/bin/env python3
# patch_atomics_sleep_js.py <js-file>
# The emscripten build emits an ABORT stub for __emscripten_atomics_sleep
# (undefined symbol -> `abort("missing function: ...")`, .stub = true). The game's
# task-graph/pthread scheduler calls it for timed sleeps, so the stub aborts the
# worker on first use. Replace it with a real SharedArrayBuffer Atomics.wait sleep.
# Idempotent: running on an already-patched (.stub = false) file is a no-op.
import re, sys

REAL = (
    'function ___emscripten_atomics_sleep(ms) {\n'
    '  try {\n'
    '    var a = ___emscripten_atomics_sleep._sab || '
    '(___emscripten_atomics_sleep._sab = new Int32Array(new SharedArrayBuffer(4)));\n'
    '    Atomics.wait(a, 0, 0, Math.max(0, +ms || 0));\n'
    '  } catch (e) {}\n'
    '}'
)

def main(path):
    s = open(path).read()
    if 'atomics_sleep.stub = false' in s:
        print('already patched'); return
    # replace the abort-stub function body
    s2 = re.sub(
        r'function ___emscripten_atomics_sleep\([^)]*\)\s*\{\s*abort\("missing function: __emscripten_atomics_sleep"\);\s*\}',
        REAL, s, count=1)
    # flip the stub flag
    s2 = s2.replace('___emscripten_atomics_sleep.stub = true;',
                    '___emscripten_atomics_sleep.stub = false;', 1)
    if s2 == s:
        print('!! patch anchors not found — js format changed', file=sys.stderr); sys.exit(2)
    open(path, 'w').write(s2)
    print('patched OK')

if __name__ == '__main__':
    main(sys.argv[1])
