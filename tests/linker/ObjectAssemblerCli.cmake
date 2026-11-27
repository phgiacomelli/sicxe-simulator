file(MAKE_DIRECTORY "${WORK_DIR}")
configure_file("${FIXTURES}/main.asm" "${WORK_DIR}/main source.asm" COPYONLY)
file(REMOVE "${WORK_DIR}/main source.obj")
execute_process(
    COMMAND "${ASSEMBLER}" "main source.asm"
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Object assembly failed (${status}): ${errors}")
endif()
file(READ "${WORK_DIR}/main source.obj" object)
set(expected [=[HMAIN  000000000016
DVALUE 00000A
RTWICE_
T000000134B1000000F200C3F2FFD000007FFFFF600000A
M00000105+TWICE_
M00000D06-MAIN__
M00000D06+TWICE_
M00001006+MAIN__
M00001006+TWICE_
E000000
]=])
string(REPLACE "_" " " expected "${expected}")
if(NOT object STREQUAL expected OR NOT output MATCHES "Objeto: main source.obj")
    message(FATAL_ERROR "Unexpected object or exit message:\n${object}\n${output}")
endif()
