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

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(SICXESimulator
    src/main.cpp

    src/ui/MainWindow.cpp
    src/ui/MainWindow.hpp

    src/core/Machine.cpp
    src/core/Machine.hpp

    src/core/Memory.cpp
    src/core/Memory.hpp

    src/core/RegisterFile.cpp
    src/core/RegisterFile.hpp

    src/core/Decoder.cpp
    src/core/Decoder.hpp

    src/core/AddressResolver.cpp
    src/core/AddressResolver.hpp

    src/core/Executor.cpp
    src/core/Executor.hpp
)

target_include_directories(SICXESimulator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(SICXESimulator
    PRIVATE
        Qt6::Widgets
)


include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(SICXESimulatorTests
    tests/MemoryTest.cpp
    src/core/Memory.cpp
    src/core/Memory.hpp
)

target_include_directories(SICXESimulatorTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(SICXESimulatorTests
    PRIVATE
        GTest::gtest_main
)

include(GoogleTest)

gtest_discover_tests(SICXESimulatorTests)
