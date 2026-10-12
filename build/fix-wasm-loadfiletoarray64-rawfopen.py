#!/usr/bin/env python3
# WebGPU MENU FIX (2026-08-14): raw-fopen MEMFS fallback for LoadFileToArray(TArray64).
#
# The engine now boots past materials into Slate/menu init, but crashes (wasm memory
# access out of bounds) because Slate UI resources fail to load: "Could not load file
# for Slate resource: Checkerboard.png" / "Unable to find Brush 'WhiteBrush'". The
# files ARE in the VFS, but Slate loads them via FImageUtils::LoadImage ->
# FFileHelper::LoadFileToArray(TArray64<uint8>&, ...), and only the TArray<uint8>
# overload got codex's wasm raw-fopen fallback -- the TArray64 one still uses
# CreateFileReader, which returns null on emscripten MEMFS. Missing CoreStyle brushes
# then null-deref -> crash. Mirror the fallback into the TArray64 overload (also fixes
# font loading via FontCacheCompositeFont, which uses the same call).
#
# Runtime Core -> wasm rebuild + atomics shim + pull.
import io, sys

F = "/mnt/vms/ss-build/UnrealEngine/Engine/Source/Runtime/Core/Private/Misc/FileHelper.cpp"
s = io.open(F, encoding="utf-8").read()

if "wndr wasm raw-fopen fallback (TArray64)" in s:
    print("already patched"); sys.exit(0)

# Target the TArray64 overload's if(!Reader) block (unique: followed by int64 TotalSize = Reader->TotalSize();)
old = (
    "\tFArchive* Reader = IFileManager::Get().CreateFileReader(Filename, Flags);\n"
    "\tif (!Reader)\n"
    "\t{\n"
    "\t\tif (!(Flags & FILEREAD_Silent))\n"
    "\t\t{\n"
    "\t\t\tUE_LOGF(LogStreaming, Warning, \"Failed to read file '%ls' error.\", Filename);\n"
    "\t\t}\n"
    "\t\treturn false;\n"
    "\t}\n"
    "\tint64 TotalSize = Reader->TotalSize();\n"
)
new = (
    "\tFArchive* Reader = IFileManager::Get().CreateFileReader(Filename, Flags);\n"
    "\tif (!Reader)\n"
    "\t{\n"
    "#if PLATFORM_WASM\n"
    "\t\t// wndr wasm raw-fopen fallback (TArray64): CreateFileReader returns null on emscripten\n"
    "\t\t// MEMFS though raw POSIX works. Needed for Slate PNGs/fonts via FImageUtils::LoadImage.\n"
    "\t\t{\n"
    "\t\t\tFILE* _fp = fopen(TCHAR_TO_UTF8(Filename), \"rb\");\n"
    "\t\t\tif (_fp)\n"
    "\t\t\t{\n"
    "\t\t\t\tfseek(_fp, 0, SEEK_END); long _sz = ftell(_fp); fseek(_fp, 0, SEEK_SET);\n"
    "\t\t\t\tif (_sz >= 0)\n"
    "\t\t\t\t{\n"
    "\t\t\t\t\tResult.Reset((int64)_sz + 2); Result.AddUninitialized((int64)_sz);\n"
    "\t\t\t\t\tsize_t _rd = (_sz > 0) ? fread(Result.GetData(), 1, (size_t)_sz, _fp) : 0;\n"
    "\t\t\t\t\tfclose(_fp);\n"
    "\t\t\t\t\tif ((long)_rd == _sz) { return true; }\n"
    "\t\t\t\t}\n"
    "\t\t\t\telse { fclose(_fp); }\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "#endif\n"
    "\t\tif (!(Flags & FILEREAD_Silent))\n"
    "\t\t{\n"
    "\t\t\tUE_LOGF(LogStreaming, Warning, \"Failed to read file '%ls' error.\", Filename);\n"
    "\t\t}\n"
    "\t\treturn false;\n"
    "\t}\n"
    "\tint64 TotalSize = Reader->TotalSize();\n"
)
if old not in s:
    print("TArray64 anchor not found", file=sys.stderr); sys.exit(1)
s = s.replace(old, new, 1)
io.open(F, "w", encoding="utf-8").write(s)
print("patched FileHelper.cpp: raw-fopen MEMFS fallback for LoadFileToArray(TArray64)")
