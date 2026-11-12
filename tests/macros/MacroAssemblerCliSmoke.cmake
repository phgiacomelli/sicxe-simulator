# Real executables, fixed working-directory output and paths containing spaces.
foreach(required MACRO_ASSEMBLER ASSEMBLER FIXTURE EXPECTED_SOURCE WORK_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "Missing ${required}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${WORK_DIR}")
set(binary "${WORK_DIR}/macro output.bin")
set(listing "${WORK_DIR}/macro output.lst")
set(manual_binary "${WORK_DIR}/manual output.bin")
set(manual_listing "${WORK_DIR}/manual output.lst")
set(expanded "${WORK_DIR}/MASMAPRG.ASM")
file(REMOVE "${binary}" "${listing}" "${manual_binary}" "${manual_listing}")
file(WRITE "${expanded}" "previous source\n")
execute_process(
    COMMAND "${MACRO_ASSEMBLER}" "${FIXTURE}" -o "${binary}" -l "${listing}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors
)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "Macro assembler failed (${status}): ${errors}")
endif()
file(READ "${EXPECTED_SOURCE}" expected)
file(READ "${expanded}" actual)
if(NOT actual STREQUAL expected OR NOT output MATCHES "0x000100")
    message(FATAL_ERROR "Expanded source/load differs from the manual expectation")
endif()
execute_process(
    COMMAND "${ASSEMBLER}" "${EXPECTED_SOURCE}" -o "${manual_binary}" -l "${manual_listing}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE manual_status ERROR_VARIABLE manual_errors
    OUTPUT_QUIET
)
if(NOT manual_status EQUAL 0)
    message(FATAL_ERROR "Direct assembler compatibility failed: ${manual_errors}")
endif()
file(READ "${binary}" image HEX)
file(READ "${manual_binary}" manual_image HEX)
file(READ "${listing}" rendered)
file(READ "${manual_listing}" manual_rendered)
string(LENGTH "${image}" length)
if(NOT image STREQUAL manual_image OR NOT length EQUAL 8320 OR
   NOT rendered STREQUAL manual_rendered)
    message(FATAL_ERROR "Binary/listing differs from manually expanded assembly")
endif()
file(WRITE "${WORK_DIR}/invalid.asm" " MCEND\n")
file(WRITE "${expanded}" "preserve on expansion failure\n")
execute_process(
    COMMAND "${MACRO_ASSEMBLER}" "${WORK_DIR}/invalid.asm"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE invalid_status OUTPUT_QUIET ERROR_QUIET
)
file(READ "${expanded}" preserved)
if(NOT invalid_status EQUAL 1 OR NOT preserved STREQUAL "preserve on expansion failure\n" OR
   EXISTS "${WORK_DIR}/invalid.bin" OR EXISTS "${WORK_DIR}/invalid.lst")
    message(FATAL_ERROR "Expansion failure changed the previous fixed source or created outputs")
endif()
# Existing .bin/.lst cause failure, but the newly expanded source must remain.
execute_process(
    COMMAND "${MACRO_ASSEMBLER}" "${FIXTURE}" -o "${binary}" -l "${listing}"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE repeated_status OUTPUT_QUIET ERROR_QUIET
)
file(READ "${expanded}" replaced)
file(READ "${binary}" unchanged HEX)
if(NOT repeated_status EQUAL 1 OR NOT replaced STREQUAL expected OR NOT unchanged STREQUAL image)
    message(FATAL_ERROR "Output failure did not retain the new expansion and previous binary")
endif()
