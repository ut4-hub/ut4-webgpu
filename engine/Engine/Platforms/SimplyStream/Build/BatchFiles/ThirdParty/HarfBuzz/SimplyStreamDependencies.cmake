# Keep the SimplyStream HarfBuzz archive on the same dependency versions that
# its platform rules link. UE's generic BuildForUE file selects the newer
# ICU 78 and FreeType 2.14 headers before project(), so override those choices
# through CMake's project include hook without modifying engine-owned sources.

if(NOT IS_DIRECTORY "${SIMPLYSTREAM_HARFBUZZ_ICU_ROOT}/include")
	message(FATAL_ERROR "SimplyStream HarfBuzz ICU headers are missing: ${SIMPLYSTREAM_HARFBUZZ_ICU_ROOT}")
endif()

if(NOT IS_DIRECTORY "${SIMPLYSTREAM_HARFBUZZ_FREETYPE_ROOT}/include")
	message(FATAL_ERROR "SimplyStream HarfBuzz FreeType headers are missing: ${SIMPLYSTREAM_HARFBUZZ_FREETYPE_ROOT}")
endif()

set(ICU_ROOT "${SIMPLYSTREAM_HARFBUZZ_ICU_ROOT}")
set(FREETYPE2_ROOT "${SIMPLYSTREAM_HARFBUZZ_FREETYPE_ROOT}")
