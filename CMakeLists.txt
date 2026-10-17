cmake_minimum_required(VERSION 3.21)

project(SICXESimulator
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CLANG_FORMAT clang-format)

if(CLANG_FORMAT)
    file(GLOB_RECURSE FORMAT_FILES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_SOURCE_DIR}/src/*.h"
        "${CMAKE_SOURCE_DIR}/tests/*.cpp"
        "${CMAKE_SOURCE_DIR}/tests/*.hpp"
        "${CMAKE_SOURCE_DIR}/tests/*.h"
    )

    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${FORMAT_FILES}
        COMMENT "Formatting C++ source files"
    )
endif()

option(SICXE_BUILD_QT "Build Qt application and controller" ON)

add_library(SICXECore STATIC
    src/core/Machine.cpp
    src/core/Machine.hpp

    src/core/Memory.cpp
    src/core/Memory.hpp

    src/core/RegisterFile.cpp
    src/core/RegisterFile.hpp
    src/core/Architecture.hpp

    src/core/Decoder.cpp
    src/core/Decoder.hpp
    src/core/Instruction.hpp

    src/core/AddressResolver.cpp
    src/core/AddressResolver.hpp
    src/core/Operand.hpp
    src/core/ExecutionEffect.hpp

    src/core/Executor.cpp
    src/core/Executor.hpp

    src/core/EffectValidation.cpp
    src/core/EffectValidation.hpp
    src/core/MachineFault.hpp

    src/core/execution/DataOperations.cpp
    src/core/execution/DataOperations.hpp
    src/core/execution/ArithmeticOperations.cpp
    src/core/execution/ArithmeticOperations.hpp
    src/core/execution/ControlOperations.cpp
    src/core/execution/ControlOperations.hpp
    src/core/execution/OperationSupport.hpp
)

target_include_directories(SICXECore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_library(sicxe_input
    src/application/HexParser.cpp
    src/application/RawBinaryReader.cpp
)
target_include_directories(sicxe_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(sicxe_assembler STATIC
    src/assembler/SourceParser.cpp
    src/assembler/SourceParser.hpp
    src/assembler/SourceLine.hpp
    src/assembler/AssemblyDiagnostic.hpp
    src/assembler/AssemblyResult.hpp
)
target_include_directories(sicxe_assembler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(SICXE_BUILD_QT)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
    qt_standard_project_setup()
    add_library(sicxe_application
        src/application/MachineController.cpp
        src/application/MachineController.hpp
    )
    target_link_libraries(sicxe_application PUBLIC SICXECore sicxe_input Qt6::Core)

    add_library(sicxe_ui
        src/ui/MainWindow.cpp
        src/ui/MainWindow.hpp
        src/ui/ExecutionPanel.cpp
        src/ui/ExecutionPanel.hpp
        src/ui/RegisterPanel.cpp
        src/ui/RegisterPanel.hpp
        src/ui/MemoryPanel.cpp
        src/ui/MemoryPanel.hpp
        src/ui/FaultPanel.cpp
        src/ui/FaultPanel.hpp
        src/ui/InputFaultText.cpp
        src/ui/InputFaultText.hpp
        src/ui/ProgramLoadDialog.cpp
        src/ui/ProgramLoadDialog.hpp
    )
    target_link_libraries(sicxe_ui PUBLIC sicxe_application Qt6::Widgets)

    qt_add_executable(SICXESimulator src/main.cpp)
    target_link_libraries(SICXESimulator PRIVATE sicxe_ui)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(GTest CONFIG QUIET)

    if(NOT GTest_FOUND)
        include(FetchContent)

        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
        )

        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(SICXESimulatorTests
        tests/DataOperationsTest.cpp
        tests/ArithmeticOperationsTest.cpp
        tests/ControlOperationsTest.cpp
        tests/AddressResolverTest.cpp
        tests/DecoderTest.cpp
        tests/EffectValidationTest.cpp
        tests/ExecutionEffectTest.cpp
        tests/ExecutorTest.cpp
        tests/MachineTest.cpp
        tests/MemoryTest.cpp
        tests/RegisterFileTest.cpp
        tests/application/HexParserTest.cpp
        tests/application/RawBinaryReaderTest.cpp
    )
    target_link_libraries(SICXESimulatorTests PRIVATE SICXECore sicxe_input GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(SICXESimulatorTests)

    if(SICXE_BUILD_QT)
        find_package(Qt6 REQUIRED COMPONENTS Test)
        add_executable(MachineControllerTests tests/application/MachineControllerTest.cpp)
        target_link_libraries(MachineControllerTests PRIVATE sicxe_application Qt6::Test)
        add_test(NAME MachineControllerTests COMMAND MachineControllerTests)

        add_executable(InterfaceTests tests/ui/InterfaceTest.cpp)
        target_link_libraries(InterfaceTests PRIVATE sicxe_ui Qt6::Test)
        add_test(NAME InterfaceTests COMMAND InterfaceTests)
        set_tests_properties(InterfaceTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

        add_executable(ProgramAcceptanceTests tests/acceptance/ProgramAcceptanceTest.cpp)
        target_link_libraries(ProgramAcceptanceTests PRIVATE sicxe_application Qt6::Test)
        add_test(NAME ProgramAcceptanceTests COMMAND ProgramAcceptanceTests)
    endif()
endif()
