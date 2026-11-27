include("${CMAKE_CURRENT_LIST_DIR}/ObjectAssemblerCli.cmake")
file(REMOVE "${WORK_DIR}/twice.obj" "${WORK_DIR}/linked.obj" "${WORK_DIR}/linked.bin"
            "${WORK_DIR}/from object.bin")

function(run command)
    execute_process(COMMAND "${command}" ${ARGN}
        WORKING_DIRECTORY "${WORK_DIR}"
        RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "${command} ${ARGN} failed (${status}): ${errors}")
    endif()
    set(last_output "${output}" PARENT_SCOPE)
endfunction()

run("${ASSEMBLER}" "${FIXTURES}/twice.asm" --output twice.obj)
file(READ "${WORK_DIR}/twice.obj" library)
set(expected_library [=[HLIB   000000000009
DTWICE 000000
RVALUE_
T000000090310000090004F0000
M00000105+VALUE_
E
]=])
string(REPLACE "_" " " expected_library "${expected_library}")
if(NOT library STREQUAL expected_library)
    message(FATAL_ERROR "Unexpected library object:\n${library}")
endif()

run("${LINKER}" --relocatable "main source.obj" twice.obj)
file(READ "${WORK_DIR}/linked.obj" linked)
set(expected_linked [=[HMAIN  00000000001F
T000000134B1000160F200C3F2FFD00000700000C000020
T000016090310000A90004F0000
M00000105+MAIN__
M00001006+MAIN__
M00001006+MAIN__
M00001705+MAIN__
E000000
]=])
string(REPLACE "_" " " expected_linked "${expected_linked}")
if(NOT linked STREQUAL expected_linked OR EXISTS "${WORK_DIR}/linked.bin")
    message(FATAL_ERROR "Unexpected relocatable output:\n${linked}")
endif()

run("${LINKER}" --absolute 0x100 "main source.obj" twice.obj)
set(direct_report "${last_output}")
run("${LINKER}" --absolute 256 linked.obj -o "from object.bin")
file(READ "${WORK_DIR}/linked.bin" direct HEX)
file(READ "${WORK_DIR}/from object.bin" reloaded HEX)
set(expected_hex "4b1001160f200c3f2ffd00000700000c0002200000000310010a90004f0000")
if(NOT direct STREQUAL expected_hex OR NOT reloaded STREQUAL direct)
    message(FATAL_ERROR "Unexpected absolute bytes:\n${direct}\n${reloaded}")
endif()
foreach(report direct_report last_output)
    string(REGEX MATCH "Carga: (0x[0-9A-F]+)\nPC inicial: (0x[0-9A-F]+)" placement "${${report}}")
    if(NOT placement OR NOT CMAKE_MATCH_1 STREQUAL "0x000100" OR
       NOT CMAKE_MATCH_2 STREQUAL "0x000100")
        message(FATAL_ERROR "Unexpected load/PC:\n${${report}}")
    endif()
    set(load "${CMAKE_MATCH_1}")
    set(pc "${CMAKE_MATCH_2}")
endforeach()
run("${CHECK_IMAGE}" "${WORK_DIR}/linked.bin" "${load}" "${pc}")
