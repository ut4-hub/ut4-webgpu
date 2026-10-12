cmake_minimum_required(VERSION 3.6)
project(SimplyStreamHarfBuzz NONE)

if(NOT IS_DIRECTORY "${SIMPLYSTREAM_HARFBUZZ_UPSTREAM}")
	message(FATAL_ERROR "SimplyStream HarfBuzz upstream is missing: ${SIMPLYSTREAM_HARFBUZZ_UPSTREAM}")
endif()

# UE's generic BuildForUE glob also captures generator/test translation units
# that define their own main(). Build the upstream target, then remove the six
# known non-library sources before CMake generates the archive.
add_subdirectory("${SIMPLYSTREAM_HARFBUZZ_UPSTREAM}" upstream)
get_target_property(HARFBUZZ_SOURCES harfbuzz SOURCES)
set(FILTERED_HARFBUZZ_SOURCES "${HARFBUZZ_SOURCES}")
set(REMOVED_GENERATORS 0)
foreach(SOURCE_FILE IN LISTS HARFBUZZ_SOURCES)
	get_filename_component(SOURCE_NAME "${SOURCE_FILE}" NAME)
	if(SOURCE_NAME MATCHES "^(dump-(indic|khmer|myanmar|use)-data|test-(iter|unicode-ranges))\\.cc$")
		list(REMOVE_ITEM FILTERED_HARFBUZZ_SOURCES "${SOURCE_FILE}")
		math(EXPR REMOVED_GENERATORS "${REMOVED_GENERATORS} + 1")
	endif()
endforeach()

if(NOT REMOVED_GENERATORS EQUAL 6)
	message(FATAL_ERROR "Expected six HarfBuzz generator/test sources, removed ${REMOVED_GENERATORS}")
endif()
set_property(TARGET harfbuzz PROPERTY SOURCES "${FILTERED_HARFBUZZ_SOURCES}")
