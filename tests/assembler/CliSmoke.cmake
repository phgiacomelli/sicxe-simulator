# Exercise the real executable, independently of the C++ entry-point tests.
file(MAKE_DIRECTORY "${WORK_DIR}")
set(binary "${WORK_DIR}/demo output.bin")
set(listing "${WORK_DIR}/demo output.lst")
file(REMOVE "${binary}" "${listing}")
execute_process(
    COMMAND "${ASSEMBLER}" "${FIXTURE}" -o "${binary}" -l "${listing}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors
)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Assembler failed (${status}): ${errors}")
endif()
file(READ "${binary}" image HEX)
string(LENGTH "${image}" length)
string(SUBSTRING "${image}" 0 112 prefix)
string(SUBSTRING "${image}" 8304 16 suffix)
string(SUBSTRING "${image}" 112 8192 reservation)
if(NOT length EQUAL 8320 OR
   NOT prefix STREQUAL "b4100100006d0005b84090103b2ff90f20174b10012f0f2013691011380340001b20090f20063f2ffd000000000000032ff72100024f0000" OR
   NOT suffix STREQUAL "0000070001294f4b" OR NOT reservation MATCHES "^0+$")
    message(FATAL_ERROR "Binary differs from hand-calculated program")
endif()
file(READ "${listing}" rendered)
if(NOT rendered MATCHES "RESERVE 4096" OR NOT output MATCHES "0x000100")
    message(FATAL_ERROR "Missing listing/load information")
endif()
execute_process(
    COMMAND "${ASSEMBLER}" "${FIXTURE}" -o "${binary}" -l "${listing}"
    RESULT_VARIABLE repeat_status OUTPUT_VARIABLE repeat_output ERROR_VARIABLE repeat_errors
)
file(READ "${binary}" unchanged HEX)
if(NOT repeat_status EQUAL 1 OR NOT unchanged STREQUAL image)
    message(FATAL_ERROR "Existing outputs were not preserved")
endif()
