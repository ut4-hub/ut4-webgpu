#!/usr/bin/env python3
# THE WINNING CONTENT-PIPELINE FIX (2026-08-13).
#
# In standalone WebGPU/wasm the SimplyStream IAS platform IO dispatcher is installed
# as GPlatformIoDispatcher, but it needs a platform HTTP server and HANGS inside
# OpenFile() the moment IoStore tries to mount a container (backend->Mount deadlocks,
# not a crash). Force a raw-fopen-patched *Generic* platform IO dispatcher on wasm,
# both at Create() time and — critically — in Get(), which is the accessor the
# container-mount path actually calls. Patching Create() alone is not enough because
# the IAS dispatcher may already be set; Get() overrides at the point of use.
#
# Pairs with:
#   fix-genericio-rawfopen.py        (GenericPlatformIoDispatcher.cpp raw fopen)
#   fix-fileiodispatcher-rawfopen.py (FileIoDispatcherBackend.cpp raw fopen)
#   FWasmRawFileHandle in IoStore.cpp (TOC read)
# Together these mount .utoc/.ucas IoStore containers from emscripten MEMFS.
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/Core/Internal/IO/PlatformIoDispatcher.cpp"
s = io.open(F, encoding="utf-8").read()

if "GWasmGeneric" in s:
    print("Get() already patched");
else:
    get_old = (
        "IPlatformIoDispatcher& FPlatformIoDispatcher::Get()\n"
        "{\n"
        "\treturn *GPlatformIoDispatcher;\n"
        "}\n"
    )
    get_new = (
        "IPlatformIoDispatcher& FPlatformIoDispatcher::Get()\n"
        "{\n"
        "#if PLATFORM_WASM\n"
        "\t// Standalone: the SimplyStream IAS dispatcher (set as GPlatformIoDispatcher) needs a\n"
        "\t// platform HTTP server and hangs in OpenFile. Use a raw-fopen-patched Generic dispatcher.\n"
        "\tstatic TUniquePtr<IPlatformIoDispatcher> GWasmGeneric = []() {\n"
        "\t\tUE_LOG(LogCore, Warning, TEXT(\"[wndr-iodisp] Get: creating forced Generic dispatcher (wasm)\"));\n"
        "\t\tFPlatformIoDispatcherCreateParams P; P.bForceGeneric = true;\n"
        "\t\tTUniquePtr<IPlatformIoDispatcher> D = MakeGenericPlatformIoDispatcher(MoveTemp(P));\n"
        "\t\tif (D.IsValid()) { D->Initialize(); }\n"
        "\t\treturn D;\n"
        "\t}();\n"
        "\treturn *GWasmGeneric;\n"
        "#else\n"
        "\treturn *GPlatformIoDispatcher;\n"
        "#endif\n"
        "}\n"
    )
    if get_old not in s:
        print("Get() anchor not found — file may already differ", file=sys.stderr); sys.exit(1)
    s = s.replace(get_old, get_new, 1)

# Also force Generic at Create() time (belt-and-braces; the Get() static is authoritative).
if "[wndr-iodisp] Create: forcing Generic" not in s:
    create_old = (
        "void FPlatformIoDispatcher::Create(FPlatformIoDispatcherCreateParams&& Params)\n"
        "{\n"
    )
    create_new = (
        "void FPlatformIoDispatcher::Create(FPlatformIoDispatcherCreateParams&& Params)\n"
        "{\n"
        "#if PLATFORM_WASM\n"
        "\tUE_LOG(LogCore, Warning, TEXT(\"[wndr-iodisp] Create: forcing Generic dispatcher (wasm)\"));\n"
        "\tGPlatformIoDispatcher = MakeGenericPlatformIoDispatcher(MoveTemp(Params));\n"
        "\treturn;\n"
        "#endif\n"
    )
    if create_old in s:
        s = s.replace(create_old, create_new, 1)

io.open(F, "w", encoding="utf-8").write(s)
print("patched PlatformIoDispatcher.cpp: force Generic IO dispatcher on wasm (Get + Create)")
