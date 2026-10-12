#!/usr/bin/env bash
set -euo pipefail

# Build the static archives linked into a SimplyStream game for one pointer ABI.
# Outputs are architecture-labelled and never replace the legacy wasm64 archives.

usage()
{
	cat <<'EOF'
Usage: Build_SimplyStream_Wasm_ThirdParty.sh --memory-mode 0|2 [--jobs N] [--only name[,name...]] [--seed-platform-root DIR]

Names: blake3, ogg, vorbis, opus, png, icu, harfbuzz, freetype, rad, oodle
The vorbis target builds both libvorbis and libvorbisfile.
An optional seed root reuses only archives matching the manifest's exact SHA-256 locks.
EOF
}

MEMORY_MODE="${SIMPLYSTREAM_MEMORY_MODE:-}"
JOBS="${SIMPLYSTREAM_THIRDPARTY_JOBS:-4}"
ONLY="all"
SEED_PLATFORM_ROOT="${SIMPLYSTREAM_THIRDPARTY_SEED_ROOT:-}"

while [[ $# -gt 0 ]]; do
	case "$1" in
		--memory-mode)
			MEMORY_MODE="${2:-}"
			shift 2
			;;
		--jobs)
			JOBS="${2:-}"
			shift 2
			;;
		--only)
			ONLY="${2:-}"
			shift 2
			;;
		--seed-platform-root)
			SEED_PLATFORM_ROOT="${2:-}"
			shift 2
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			echo "Unknown argument: $1" >&2
			usage >&2
			exit 2
			;;
	esac
done

case "$MEMORY_MODE" in
	0)
		WASM_ARCH="wasm32"
		;;
	2)
		WASM_ARCH="wasm64"
		;;
	*)
		echo "--memory-mode must be 0 (native wasm32) or 2 (legacy wasm64 pointers on wasm32 memory)" >&2
		exit 2
		;;
esac

if [[ ! "$JOBS" =~ ^[1-9][0-9]*$ ]]; then
	echo "--jobs must be a positive integer" >&2
	exit 2
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PLATFORM_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
ENGINE_ROOT="$(cd -- "$PLATFORM_ROOT/../.." && pwd)"
ENGINE_THIRD_PARTY="$ENGINE_ROOT/Source/ThirdParty"
PLATFORM_THIRD_PARTY="$PLATFORM_ROOT/Source/ThirdParty"
BUILD_ROOT="${SIMPLYSTREAM_THIRDPARTY_BUILD_ROOT:-$ENGINE_ROOT/Intermediate/Build/SimplyStreamThirdParty/$WASM_ARCH}"
PREP_MANIFEST="$SCRIPT_DIR/StrataSourcePrepare.json"

for Tool in emcc em++ emcmake emar cmake ninja unzip jq sha256sum; do
	if ! command -v "$Tool" >/dev/null 2>&1; then
		echo "Required tool is not on PATH: $Tool" >&2
		exit 2
	fi
done

EMCC_DIR="$(cd -- "$(dirname -- "$(command -v emcc)")" && pwd)"
EMSCRIPTEN_UPSTREAM="$(cd -- "$EMCC_DIR/.." && pwd)"
LLVM_READOBJ="$EMSCRIPTEN_UPSTREAM/bin/llvm-readobj"
if [[ ! -x "$LLVM_READOBJ" ]]; then
	echo "Unable to locate llvm-readobj beside emcc: $LLVM_READOBJ" >&2
	exit 2
fi
LLVM_NM="$EMSCRIPTEN_UPSTREAM/bin/llvm-nm"
if [[ ! -x "$LLVM_NM" ]]; then
	echo "Unable to locate llvm-nm beside emcc: $LLVM_NM" >&2
	exit 2
fi

COMMON_FLAGS=(
	-O3
	-DNDEBUG
	"-sMEMORY64=$MEMORY_MODE"
	-pthread
	-msimd128
	-msse
	-msse2
	-msse3
	-mssse3
	-msse4.1
	-msse4.2
)
COMMON_FLAGS_STRING="${COMMON_FLAGS[*]}"

mkdir -p "$BUILD_ROOT"

should_build()
{
	local Name="$1"
	[[ "$ONLY" == "all" || ",$ONLY," == *",$Name,"* ]]
}

validate_requested_libraries()
{
	[[ "$ONLY" == "all" ]] && return
	local Requested
	local RequestedLibraries=()
	IFS=',' read -r -a RequestedLibraries <<< "$ONLY"
	for Requested in "${RequestedLibraries[@]}"; do
		case "$Requested" in
			blake3|ogg|vorbis|opus|png|icu|harfbuzz|freetype|rad|oodle) ;;
			*)
				echo "Unknown third-party library: $Requested" >&2
				exit 2
				;;
		esac
	done
}

validate_requested_libraries

reset_build_dir()
{
	local Directory="$1"
	case "$Directory" in
		"$BUILD_ROOT"/*)
			rm -rf -- "$Directory"
			mkdir -p -- "$Directory"
			;;
		*)
			echo "Refusing to reset a directory outside the third-party build root: $Directory" >&2
			exit 2
			;;
	esac
}

copy_archive()
{
	local Source="$1"
	local Destination="$2"
	if [[ ! -f "$Source" ]]; then
		echo "Expected archive was not produced: $Source" >&2
		exit 3
	fi
	mkdir -p -- "$(dirname -- "$Destination")"
	cp -f -- "$Source" "$Destination"
}

verify_archive()
{
	local Archive="$1"
	local Observed
	Observed="$($LLVM_READOBJ --file-headers "$Archive" | awk '/^Arch: / { print $2 }' | sort -u | paste -sd, -)"
	if [[ "$Observed" != "$WASM_ARCH" ]]; then
		echo "Archive ABI mismatch: expected $WASM_ARCH, observed '${Observed:-none}': $Archive" >&2
		exit 4
	fi
	local Hash
	Hash="$(sha256sum "$Archive" | awk '{ print $1 }')"
	echo "[SimplyStream third-party] $WASM_ARCH $Hash $Archive"
}

verify_archive_symbol()
{
	local Archive="$1"
	local Symbol="$2"
	if ! "$LLVM_NM" --defined-only "$Archive" 2>/dev/null | awk -v Required="$Symbol" '$NF == Required { Found = 1 } END { exit !Found }'; then
		echo "Archive is missing required symbol '$Symbol': $Archive" >&2
		exit 4
	fi
}

verify_archive_undefined_symbol()
{
	local Archive="$1"
	local Symbol="$2"
	if ! "$LLVM_NM" --undefined-only "$Archive" 2>/dev/null | awk -v Required="$Symbol" '$NF == Required { Found = 1 } END { exit !Found }'; then
		echo "Archive is missing required undefined symbol '$Symbol': $Archive" >&2
		exit 4
	fi
}

verify_archive_absent_symbol()
{
	local Archive="$1"
	local Symbol="$2"
	if "$LLVM_NM" --defined-only "$Archive" 2>/dev/null | awk -v Forbidden="$Symbol" '$NF == Forbidden { Found = 1 } END { exit !Found }'; then
		echo "Archive contains forbidden symbol '$Symbol': $Archive" >&2
		exit 4
	fi
}

locked_archive_records()
{
	local Library="$1"
	[[ -f "$PREP_MANIFEST" ]] || return 0
	jq -r --arg Library "$Library" '
		(.thirdParty.archives // [])[]
		| select(.library == $Library)
		| [.path, .sha256]
		| @tsv
	' "$PREP_MANIFEST"
}

validate_locked_archive_record()
{
	local RelativePath="$1"
	local ExpectedHash="$2"
	if [[ "$RelativePath" != Source/*.a || "$RelativePath" == *".."* || ! "$ExpectedHash" =~ ^[0-9a-f]{64}$ ]]; then
		echo "Invalid third-party archive lock: path='$RelativePath' sha256='$ExpectedHash'" >&2
		exit 2
	fi
}

seed_locked_archives()
{
	local Library="$1"
	[[ -n "$SEED_PLATFORM_ROOT" ]] || return 1
	local Records=()
	local Record
	while IFS= read -r Record; do Records+=("$Record"); done < <(locked_archive_records "$Library")
	[[ ${#Records[@]} -gt 0 ]] || return 1

	local RelativePath ExpectedHash SourceArchive ObservedHash
	for Record in "${Records[@]}"; do
		IFS=$'\t' read -r RelativePath ExpectedHash <<< "$Record"
		validate_locked_archive_record "$RelativePath" "$ExpectedHash"
		SourceArchive="$SEED_PLATFORM_ROOT/$RelativePath"
		[[ -f "$SourceArchive" ]] || return 1
		ObservedHash="$(sha256sum "$SourceArchive" | awk '{ print $1 }')"
		[[ "$ObservedHash" == "$ExpectedHash" ]] || return 1
	done

	for Record in "${Records[@]}"; do
		IFS=$'\t' read -r RelativePath _ <<< "$Record"
		copy_archive "$SEED_PLATFORM_ROOT/$RelativePath" "$PLATFORM_ROOT/$RelativePath"
	done
	echo "[SimplyStream third-party] reused hash-locked $Library archive(s) from $SEED_PLATFORM_ROOT"
}

verify_locked_outputs()
{
	local Library="$1"
	[[ -f "$PREP_MANIFEST" ]] || return 0
	local TotalLocks
	TotalLocks="$(jq -r '(.thirdParty.archives // []) | length' "$PREP_MANIFEST")"
	[[ "$TotalLocks" -gt 0 ]] || return 0

	local Records=()
	local Record
	while IFS= read -r Record; do Records+=("$Record"); done < <(locked_archive_records "$Library")
	if [[ ${#Records[@]} -eq 0 ]]; then
		echo "The source-preparation manifest has no archive lock for requested library '$Library'" >&2
		exit 4
	fi

	local RelativePath ExpectedHash Archive ObservedHash
	for Record in "${Records[@]}"; do
		IFS=$'\t' read -r RelativePath ExpectedHash <<< "$Record"
		validate_locked_archive_record "$RelativePath" "$ExpectedHash"
		Archive="$PLATFORM_ROOT/$RelativePath"
		[[ -f "$Archive" ]] || { echo "Locked archive was not produced: $Archive" >&2; exit 4; }
		verify_archive "$Archive"
		ObservedHash="$(sha256sum "$Archive" | awk '{ print $1 }')"
		if [[ "$ObservedHash" != "$ExpectedHash" ]]; then
			echo "Archive hash mismatch: expected $ExpectedHash, observed $ObservedHash: $Archive" >&2
			exit 4
		fi
	done

	case "$Library" in
		harfbuzz)
			Archive="$PLATFORM_ROOT/$(jq -r '.thirdParty.archives[] | select(.library == "harfbuzz") | .path' "$PREP_MANIFEST")"
			verify_archive_absent_symbol "$Archive" main
			verify_archive_undefined_symbol "$Archive" u_charMirror_64
			if "$LLVM_NM" --undefined-only "$Archive" 2>/dev/null | awk '$NF ~ /_78$/ { Found = 1 } END { exit !Found }'; then
				echo "HarfBuzz unexpectedly references ICU 78 symbols: $Archive" >&2
				exit 4
			fi
			;;
		rad)
			Archive="$PLATFORM_ROOT/$(jq -r '.thirdParty.archives[] | select(.library == "rad") | .path' "$PREP_MANIFEST")"
			verify_archive_symbol "$Archive" UERARadAudioDecoderOpen_1
			verify_archive_symbol "$Archive" UERARadAudioInterleave_1
			;;
	esac
}

configure_cmake()
{
	local Name="$1"
	local Source="$2"
	shift 2
	local BuildDirectory="$BUILD_ROOT/$Name"
	reset_build_dir "$BuildDirectory"
	emcmake cmake \
		-S "$Source" \
		-B "$BuildDirectory" \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		"-DCMAKE_C_FLAGS_RELEASE=$COMMON_FLAGS_STRING" \
		"-DCMAKE_CXX_FLAGS_RELEASE=$COMMON_FLAGS_STRING" \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		"$@"
}

build_blake3()
{
	local BuildDirectory="$BUILD_ROOT/blake3"
	configure_cmake blake3 "$ENGINE_THIRD_PARTY/BLAKE3/1.3.1/BuildForUE" \
		-DBLAKE3_NO_SSE2=ON \
		-DBLAKE3_NO_SSE41=ON \
		-DBLAKE3_NO_AVX2=ON \
		-DBLAKE3_NO_AVX512=ON \
		-DBLAKE3_NO_NEON=ON
	cmake --build "$BuildDirectory" --target BLAKE3 --parallel "$JOBS"
	local Destination="$PLATFORM_THIRD_PARTY/Blake3/1.3.1/lib/$WASM_ARCH/libBLAKE3.a"
	copy_archive "$BuildDirectory/libBLAKE3.a" "$Destination"
	verify_archive "$Destination"
}

copy_source_tree()
{
	local Name="$1"
	local Source="$2"
	local Destination="$BUILD_ROOT/sources/$Name"
	reset_build_dir "$Destination"
	cp -a "$Source"/. "$Destination"/
	printf '%s\n' "$Destination"
}

build_ogg()
{
	local Source
	Source="$(copy_source_tree ogg "$ENGINE_THIRD_PARTY/Ogg/libogg-1.2.2")"
	local BuildDirectory="$BUILD_ROOT/ogg"
	configure_cmake ogg "$Source" -DBUILD_SHARED_LIBS=OFF
	cmake --build "$BuildDirectory" --target ogg --parallel "$JOBS"
	local Destination="$PLATFORM_THIRD_PARTY/Ogg/libogg-1.2.2/lib/$WASM_ARCH/libogg.a"
	copy_archive "$BuildDirectory/libogg.a" "$Destination"
	verify_archive "$Destination"
}

build_vorbis()
{
	local Source
	Source="$(copy_source_tree vorbis "$ENGINE_THIRD_PARTY/Vorbis/libvorbis-1.3.2")"
	local BuildDirectory="$BUILD_ROOT/vorbis"
	local OggRoot="$BUILD_ROOT/ogg-install"
	reset_build_dir "$OggRoot"
	mkdir -p "$OggRoot/include/ogg" "$OggRoot/lib"
	cp -a "$ENGINE_THIRD_PARTY/Ogg/libogg-1.2.2/include/ogg"/. "$OggRoot/include/ogg"/
	cp -f "$PLATFORM_THIRD_PARTY/Ogg/libogg-1.2.2/lib/$WASM_ARCH/libogg.a" "$OggRoot/lib/libogg.a"
	configure_cmake vorbis "$Source" \
		-DBUILD_SHARED_LIBS=OFF \
		"-DOGG_INCLUDE_DIRS=$OggRoot/include" \
		"-DOGG_LIBRARIES=$OggRoot/lib/libogg.a"
	cmake --build "$BuildDirectory" --target vorbis vorbisfile --parallel "$JOBS"
	local DestinationRoot="$PLATFORM_THIRD_PARTY/Vorbis/libvorbis-1.3.2/lib/$WASM_ARCH"
	copy_archive "$BuildDirectory/lib/libvorbis.a" "$DestinationRoot/libvorbis.a"
	copy_archive "$BuildDirectory/lib/libvorbisfile.a" "$DestinationRoot/libvorbisfile.a"
	verify_archive "$DestinationRoot/libvorbis.a"
	verify_archive "$DestinationRoot/libvorbisfile.a"
}

build_opus()
{
	local Source
	Source="$(copy_source_tree opus "$ENGINE_THIRD_PARTY/libOpus/opus-1.1")"
	cp -f "$SCRIPT_DIR/ThirdParty/libOpus/opus-1.1/CMakeLists.txt" "$Source/CMakeLists.txt"
	local BuildDirectory="$BUILD_ROOT/opus"
	configure_cmake opus "$Source" -DBUILD_SHARED_LIBS=OFF
	cmake --build "$BuildDirectory" --target opus --parallel "$JOBS"
	local Destination="$PLATFORM_THIRD_PARTY/libOpus/opus-1.1/lib/$WASM_ARCH/libopus.a"
	copy_archive "$BuildDirectory/libopus.a" "$Destination"
	verify_archive "$Destination"
}

build_png()
{
	local Source="$ENGINE_THIRD_PARTY/libPNG/libPNG-1.6.44"
	local BuildDirectory="$BUILD_ROOT/png"
	local Sysroot="$EMSCRIPTEN_UPSTREAM/emscripten/cache/sysroot"
	local ZlibArchive="$Sysroot/lib/wasm32-emscripten/libz.a"
	if [[ ! -f "$ZlibArchive" ]]; then
		echo "The Emscripten zlib cache archive is missing: $ZlibArchive" >&2
		exit 3
	fi
	configure_cmake png "$Source" \
		-DPNG_SHARED=OFF \
		-DPNG_STATIC=ON \
		-DPNG_TESTS=OFF \
		-DPNG_TOOLS=OFF \
		-DPNG_EXECUTABLES=OFF \
		"-DZLIB_INCLUDE_DIR=$Sysroot/include" \
		"-DZLIB_LIBRARY=$ZlibArchive"
	cmake --build "$BuildDirectory" --target png_static --parallel "$JOBS"
	local Destination="$PLATFORM_THIRD_PARTY/libPNG/libPNG-1.6.44/lib/$WASM_ARCH/libpng.a"
	copy_archive "$BuildDirectory/libpng16.a" "$Destination"
	verify_archive "$Destination"
}

build_icu()
{
	local Source="$ENGINE_THIRD_PARTY/ICU/icu4c-64_1/BuildForUE"
	local BuildDirectory="$BUILD_ROOT/icu"
	configure_cmake icu "$Source" \
		-DBUILD_SHARED_LIBS=OFF \
		-DBUILD_ICU_MONOLITHIC=ON \
		-DBUILD_ICU_STATIC=ON \
		"-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=$BuildDirectory"
	cmake --build "$BuildDirectory" --target icu --parallel "$JOBS"
	local Destination="$PLATFORM_THIRD_PARTY/ICU/icu4c-64_1/lib/$WASM_ARCH/libicu.a"
	copy_archive "$BuildDirectory/libicu.a" "$Destination"
	verify_archive "$Destination"
}

build_harfbuzz()
{
	local Upstream="$ENGINE_THIRD_PARTY/HarfBuzz/harfbuzz-2.4.0/BuildForUE"
	local Source="$SCRIPT_DIR/ThirdParty/HarfBuzz"
	local BuildDirectory="$BUILD_ROOT/harfbuzz"
	configure_cmake harfbuzz "$Source" \
		-DUSE_INTEL_ATOMIC_PRIMITIVES=ON \
		"-DSIMPLYSTREAM_HARFBUZZ_UPSTREAM=$Upstream" \
		"-DCMAKE_PROJECT_INCLUDE_BEFORE=$SCRIPT_DIR/ThirdParty/HarfBuzz/SimplyStreamDependencies.cmake" \
		"-DSIMPLYSTREAM_HARFBUZZ_ICU_ROOT=$ENGINE_THIRD_PARTY/ICU/icu4c-64_1" \
		"-DSIMPLYSTREAM_HARFBUZZ_FREETYPE_ROOT=$ENGINE_THIRD_PARTY/FreeType2/FreeType2-2.10.0" \
		"-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=$BuildDirectory"
	cmake --build "$BuildDirectory" --target harfbuzz --parallel "$JOBS"
	local Destination="$PLATFORM_THIRD_PARTY/HarfBuzz/harfbuzz-2.4.0/lib/$WASM_ARCH/libharfbuzz.a"
	copy_archive "$BuildDirectory/libharfbuzz.a" "$Destination"
	verify_archive "$Destination"
	verify_archive_absent_symbol "$Destination" main
	verify_archive_undefined_symbol "$Destination" u_charMirror_64
	if "$LLVM_NM" --undefined-only "$Destination" 2>/dev/null | awk '$NF ~ /_78$/ { Found = 1 } END { exit !Found }'; then
		echo "HarfBuzz unexpectedly references ICU 78 symbols: $Destination" >&2
		exit 4
	fi
}

build_freetype()
{
	local Source="$ENGINE_THIRD_PARTY/FreeType2/FreeType2-2.10.0"
	local BuildDirectory="$BUILD_ROOT/freetype"
	local Sysroot="$EMSCRIPTEN_UPSTREAM/emscripten/cache/sysroot"
	local PngBuildDirectory="$BUILD_ROOT/png"
	local PngArchive="$PLATFORM_THIRD_PARTY/libPNG/libPNG-1.6.44/lib/$WASM_ARCH/libpng.a"
	local HarfBuzzArchive="$PLATFORM_THIRD_PARTY/HarfBuzz/harfbuzz-2.4.0/lib/$WASM_ARCH/libharfbuzz.a"
	local ExtraCFlags="$COMMON_FLAGS_STRING -I$PngBuildDirectory"
	reset_build_dir "$BuildDirectory"
	emcmake cmake \
		-S "$Source" \
		-B "$BuildDirectory" \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		"-DCMAKE_C_FLAGS_RELEASE=$ExtraCFlags" \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DFT_WITH_ZLIB=ON \
		-DFT_WITH_PNG=ON \
		-DFT_WITH_HARFBUZZ=ON \
		"-DZLIB_INCLUDE_DIR=$Sysroot/include" \
		"-DZLIB_LIBRARY=$Sysroot/lib/wasm32-emscripten/libz.a" \
		"-DPNG_PNG_INCLUDE_DIR=$ENGINE_THIRD_PARTY/libPNG/libPNG-1.6.44" \
		"-DPNG_LIBRARY=$PngArchive" \
		"-DHARFBUZZ_INCLUDE_DIRS=$ENGINE_THIRD_PARTY/HarfBuzz/harfbuzz-2.4.0/src" \
		"-DHARFBUZZ_LIBRARIES=$HarfBuzzArchive"
	cmake --build "$BuildDirectory" --target freetype --parallel "$JOBS"
	local Destination="$PLATFORM_THIRD_PARTY/FreeType2/FreeType2-2.10.0/lib/$WASM_ARCH/libfreetype.a"
	copy_archive "$BuildDirectory/libfreetype.a" "$Destination"
	verify_archive "$Destination"
}

compile_rad_source()
{
	local Source="$1"
	local Object="$2"
	shift 2
	local CompatHeader="$SCRIPT_DIR/ThirdParty/RadAudio/rad_emscripten_compat.h"
	case "$Source" in
		*.c)
			emcc "${COMMON_FLAGS[@]}" -include "$CompatHeader" -DUSING_EGT -DRADA_WRAP=UERA -DRADAUDIO_WRAP=UERA "$@" -c "$Source" -o "$Object"
			;;
		*.cpp)
			em++ "${COMMON_FLAGS[@]}" -std=c++17 -include "$CompatHeader" -DUSING_EGT -DRADA_WRAP=UERA -DRADAUDIO_WRAP=UERA "$@" -c "$Source" -o "$Object"
			;;
		*)
			echo "Unsupported RAD source extension: $Source" >&2
			exit 3
			;;
	esac
}

build_rad()
{
	local SourceRoot="$ENGINE_ROOT/Source/Runtime/RadAudioCodec/SDK/Src"
	local RadAudio="$SourceRoot/RadAudio"
	local RadA="$SourceRoot/RadA"
	local BuildDirectory="$BUILD_ROOT/rad"
	reset_build_dir "$BuildDirectory"
	local Includes=(-I"$SourceRoot/../Include" -I"$RadAudio" -I"$RadA")
	local Sources=(
		"$RadAudio/radaudio_decoder.c"
		"$RadAudio/radaudio_decoder_sse2.c"
		"$RadAudio/radaudio_decoder_sse4.c"
		"$RadAudio/radaudio_decoder_avx2.c"
		"$RadAudio/radaudio_decoder_neon.c"
		"$SCRIPT_DIR/ThirdParty/RadAudio/radaudio_interleave.c"
		"$RadA/rada_decode.cpp"
		"$RadAudio/radaudio_mdct.cpp"
		"$RadAudio/radaudio_mdct_neon.cpp"
		"$RadAudio/radaudio_mdct_sse2.cpp"
		"$RadAudio/radaudio_mdct_avx2.cpp"
	)
	local Objects=()
	local Source
	for Source in "${Sources[@]}"; do
		local Object="$BuildDirectory/$(basename -- "$Source").o"
		local VariantFlags=()
		case "$Source" in
			*_sse2.c|*_sse2.cpp) VariantFlags=(-msse2) ;;
			*_sse4.c|*_sse4.cpp) VariantFlags=(-msse4.1) ;;
			*_avx2.c|*_avx2.cpp) VariantFlags=(-mavx2) ;;
		esac
		compile_rad_source "$Source" "$Object" "${Includes[@]}" "${VariantFlags[@]}"
		Objects+=("$Object")
	done
	local Destination="$PLATFORM_ROOT/Source/Runtime/SDK/Lib/libradaudio_decoder_$WASM_ARCH.a"
	mkdir -p -- "$(dirname -- "$Destination")"
	remove_if_present "$Destination"
	emar rcs "$Destination" "${Objects[@]}"
	verify_archive "$Destination"
	verify_archive_symbol "$Destination" UERARadAudioDecoderOpen_1
	verify_archive_symbol "$Destination" UERARadAudioInterleave_1
}

remove_if_present()
{
	local Path="$1"
	if [[ -e "$Path" ]]; then
		rm -f -- "$Path"
	fi
}

build_oodle()
{
	local Version="2.9.14"
	local DefaultZip="$ENGINE_ROOT/Source/Runtime/OodleDataCompression/Sdks/$Version/src/oodle2_src_for_unreal_data_$Version.zip"
	local Zip="${SIMPLYSTREAM_OODLE_SOURCE_ZIP:-$DefaultZip}"
	if [[ ! -f "$Zip" ]]; then
		echo "Oodle source archive is missing: $Zip (set SIMPLYSTREAM_OODLE_SOURCE_ZIP to the licensed 2.9.14 source zip)" >&2
		exit 3
	fi
	local BuildDirectory="$BUILD_ROOT/oodle"
	reset_build_dir "$BuildDirectory"
	unzip -q "$Zip" -d "$BuildDirectory/source"
	local SourceRoot="$BuildDirectory/source/oodle2"
	local StableSourceRoot="/simplystream-third-party/oodle2"
	local Names=(
		histogram entropysets longrangematcher ctmf_avx2 lzasserts lzb newlzf newlz
		newlzhc newlz_tans newlz_simd newlz_histo newlz_shared newlz_vtable
		newlz_arrays newlz_arrays_rle newlz_arrays_huff newlz_arrays_tans
		newlz_block_coders newlz_block_coders_ssse3 newlz_multiarrays
		newlz_multiarrays_sse4 newlz_offsets newlz_speedfit newlz_simd_sse4
		newlzhc_sse4 rrvarbits suffixtriematcher suffixtrie rrvarbitcodes
		rrsimpleprof rrmemutil rrmem rrmath rrlzh_lzhlw_shared rrlogutil rrlinknode
		rrlog rrhuffmandecode rrhuffman rrcompressutil rrhashes rrrand rrbytepacking
		rrbighash rrbase rrarenaallocator oodleversion oodlemalloc oodlelzpub
		oodlelzlegacyvtable oodlelzcompressors oodlejob oodlehandle oodlecoreplugins
		oodlecoreinit oodlecore oodleconfigvalues oodlebase threadprofiler biglog2table
	)
	# Oodle retains __FILE__ in diagnostics even with NDEBUG. Passing absolute checkout paths made
	# otherwise-identical archives hash differently in the source-cook and engine-release worktrees.
	# Compile from the extracted source root and map any path Clang still materializes to one stable
	# virtual root. Pin builtin date/time expansion and archive headers for the same reason.
	local Includes=(-Ibase -Icore -Icore/public)
	local ReproducibleFlags=(
		"-ffile-prefix-map=$SourceRoot=$StableSourceRoot"
		"-fmacro-prefix-map=$SourceRoot=$StableSourceRoot"
		"-fdebug-prefix-map=$SourceRoot=$StableSourceRoot"
	)
	local Objects=()
	local Name
	for Name in "${Names[@]}"; do
		mapfile -t Matches < <(find "$SourceRoot" -type f -name "$Name.cpp" -print)
		if [[ ${#Matches[@]} -ne 1 ]]; then
			echo "Expected exactly one Oodle source for $Name.cpp, found ${#Matches[@]}" >&2
			exit 3
		fi
		local Object="$BuildDirectory/$Name.cpp.o"
		local RelativeSource="${Matches[0]#${SourceRoot}/}"
		(
			cd "$SourceRoot"
			SOURCE_DATE_EPOCH=946684800 LC_ALL=C TZ=UTC \
				em++ "${COMMON_FLAGS[@]}" "${ReproducibleFlags[@]}" -std=c++17 \
					-fno-exceptions -fno-rtti -DOODLE_BUILDING_LIB "${Includes[@]}" \
					-c "$RelativeSource" -o "$Object"
		)
		Objects+=("$Object")
	done
	local Destination="$PLATFORM_ROOT/Source/Runtime/OodleDataCompression/Sdks/$Version/lib/$WASM_ARCH/liboodle.a"
	mkdir -p -- "$(dirname -- "$Destination")"
	remove_if_present "$Destination"
	LC_ALL=C TZ=UTC emar rcsD "$Destination" "${Objects[@]}"
	if grep -aFq "$BuildDirectory" "$Destination"; then
		echo "Oodle archive contains its physical build root: $BuildDirectory" >&2
		exit 4
	fi
	verify_archive "$Destination"
}

declare -a BuildOrder=(blake3 ogg vorbis opus png icu harfbuzz freetype rad oodle)
for Library in "${BuildOrder[@]}"; do
	if should_build "$Library"; then
		if seed_locked_archives "$Library"; then
			:
		else
			echo "[SimplyStream third-party] building $Library for $WASM_ARCH (MEMORY64=$MEMORY_MODE)"
			"build_$Library"
		fi
		verify_locked_outputs "$Library"
	fi
done

echo "[SimplyStream third-party] completed $WASM_ARCH archive build"
