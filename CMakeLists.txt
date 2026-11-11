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
    src/assembler/Assembler.cpp
    src/assembler/Assembler.hpp
    src/assembler/PassTwo.cpp
    src/assembler/PassTwo.hpp
    src/assembler/RawImageWriter.cpp
    src/assembler/RawImageWriter.hpp
    src/assembler/ListingWriter.cpp
    src/assembler/ListingWriter.hpp
    src/assembler/AddressLimits.hpp
    src/assembler/SourceParser.cpp
    src/assembler/SourceParser.hpp
    src/assembler/SourceLine.hpp
    src/assembler/AssemblyDiagnostic.hpp
    src/assembler/AssemblyResult.hpp
    src/assembler/SymbolTable.cpp
    src/assembler/SymbolTable.hpp
    src/assembler/OperandValue.cpp
    src/assembler/OperandValue.hpp
    src/assembler/InstructionCatalog.cpp
    src/assembler/InstructionCatalog.hpp
    src/assembler/InstructionOperand.cpp
    src/assembler/InstructionOperand.hpp
    src/assembler/Format2Encoder.cpp
    src/assembler/Format2Encoder.hpp
    src/assembler/LocatedLine.hpp
    src/assembler/PassOne.cpp
    src/assembler/PassOne.hpp
    src/assembler/DataDirective.cpp
    src/assembler/DataDirective.hpp
    src/assembler/InstructionEncoder.cpp
    src/assembler/InstructionEncoder.hpp
)
target_include_directories(sicxe_assembler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(sicxe_assembler_cli STATIC
    src/assembler/AssemblerCli.cpp
    src/assembler/AssemblerCli.hpp
    src/assembler/OutputFiles.cpp
    src/assembler/OutputFiles.hpp
)
target_link_libraries(sicxe_assembler_cli PUBLIC sicxe_assembler)
add_executable(SICXEAssembler src/assembler/main.cpp)
target_link_libraries(SICXEAssembler PRIVATE sicxe_assembler_cli)

add_library(sicxe_macros STATIC
    src/macros/SourcePosition.hpp
    src/macros/SourceLine.hpp
    src/macros/SourceLine.cpp
    src/macros/SourceScanner.hpp
    src/macros/SourceScanner.cpp
    src/macros/SourceOrigin.hpp
    src/macros/MacroDiagnostic.hpp
    src/macros/MacroResult.hpp
    src/macros/MacroProcessor.hpp
    src/macros/MacroDefinition.hpp
    src/macros/MacroDefinition.cpp
    src/macros/DefinitionCapture.hpp
    src/macros/DefinitionCapture.cpp
    src/macros/MacroTable.hpp
    src/macros/MacroTable.cpp
    src/macros/MacroArguments.hpp
    src/macros/MacroArguments.cpp
    src/macros/MacroTemplate.hpp
    src/macros/MacroTemplate.cpp
    src/macros/ParameterSubstitution.hpp
    src/macros/ParameterSubstitution.cpp
)
target_include_directories(sicxe_macros PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(sicxe_macros PRIVATE sicxe_assembler)

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

    add_executable(AssemblerTests
        tests/assembler/AssemblerTest.cpp
        tests/assembler/SourceParserTest.cpp
        tests/assembler/AssemblyContractTest.cpp
        tests/assembler/SymbolTableTest.cpp
        tests/assembler/OperandValueTest.cpp
        tests/assembler/InstructionCatalogTest.cpp
        tests/assembler/InstructionOperandTest.cpp
        tests/assembler/Format2EncoderTest.cpp
        tests/assembler/PassOneTest.cpp
        tests/assembler/DataDirectiveTest.cpp
        tests/assembler/InstructionEncoderTest.cpp
    )
    set_target_properties(AssemblerTests PROPERTIES AUTOMOC OFF AUTOUIC OFF)
    target_link_libraries(AssemblerTests PRIVATE sicxe_assembler GTest::gtest_main)
    gtest_discover_tests(AssemblerTests NO_PRETTY_VALUES)

    add_executable(MacroTests
        tests/macros/SourceScannerTest.cpp
        tests/macros/MacroContractTest.cpp
        tests/macros/DefinitionCaptureTest.cpp
        tests/macros/MacroTableTest.cpp
        tests/macros/MacroArgumentsTest.cpp
        tests/macros/ParameterSubstitutionTest.cpp
    )
    set_target_properties(MacroTests PROPERTIES AUTOMOC OFF AUTOUIC OFF)
    target_link_libraries(MacroTests PRIVATE sicxe_macros GTest::gtest_main)
    gtest_discover_tests(MacroTests NO_PRETTY_VALUES)

    add_executable(AssemblerCliTests tests/assembler/AssemblerCliTest.cpp)
    set_target_properties(AssemblerCliTests PROPERTIES AUTOMOC OFF AUTOUIC OFF)
    target_link_libraries(AssemblerCliTests PRIVATE sicxe_assembler_cli GTest::gtest_main)
    gtest_discover_tests(AssemblerCliTests NO_PRETTY_VALUES)
    add_test(NAME AssemblerCliHelp COMMAND SICXEAssembler --help)
    set_tests_properties(AssemblerCliHelp PROPERTIES PASS_REGULAR_EXPRESSION "Uso: SICXEAssembler")
    add_test(NAME AssemblerCliSmoke COMMAND ${CMAKE_COMMAND}
        -DASSEMBLER=$<TARGET_FILE:SICXEAssembler>
        -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/assembler/fixtures/sum-and-table.asm
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/assembler-cli-smoke
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assembler/CliSmoke.cmake)

    # Only compatibility/execution tests depend on the VM; the assembler stays independent.
    add_executable(AssemblerDecoderTests
        tests/assembler/AssemblerAcceptanceTest.cpp
        tests/assembler/Format2DecoderTest.cpp
        tests/assembler/InstructionEncoderIntegrationTest.cpp
    )
    set_target_properties(AssemblerDecoderTests PROPERTIES AUTOMOC OFF AUTOUIC OFF)
    target_link_libraries(AssemblerDecoderTests PRIVATE sicxe_assembler SICXECore GTest::gtest_main)
    target_compile_definitions(AssemblerDecoderTests PRIVATE
        ASSEMBLER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/assembler/fixtures")
    gtest_discover_tests(AssemblerDecoderTests NO_PRETTY_VALUES)

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
