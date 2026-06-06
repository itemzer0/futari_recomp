cmake_minimum_required(VERSION 3.25)
project(futari CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# ReXGlue SDK
# ---------------------------------------------------------------------------
set(REXGLUE_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/rexglue-sdk")
if(NOT EXISTS "${REXGLUE_SDK_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "ReXGlue SDK not found at ${REXGLUE_SDK_DIR}\n"
        "Run: git submodule update --init --recursive")
endif()
add_subdirectory("${REXGLUE_SDK_DIR}" rexglue-sdk)

# ---------------------------------------------------------------------------
# Generated PPC sources (produced by the codegen target below)
# ---------------------------------------------------------------------------
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../generated")
set(GENERATED_STUB "${GENERATED_DIR}/stub_generated.cpp")

# Create a stub so CMake can configure before codegen has run
if(NOT EXISTS "${GENERATED_STUB}")
    file(MAKE_DIRECTORY "${GENERATED_DIR}")
    file(WRITE "${GENERATED_STUB}" "// placeholder — run the codegen target first\n")
endif()

file(GLOB GENERATED_SOURCES "${GENERATED_DIR}/*.cpp")

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(futari
    src/main.cpp
    src/stubs.cpp
    ${GENERATED_SOURCES}
)

target_include_directories(futari PRIVATE
    src
    "${CMAKE_CURRENT_SOURCE_DIR}/../ppc"
    "${REXGLUE_SDK_DIR}/include"
    "${REXGLUE_SDK_DIR}/thirdparty/imgui"
)

target_compile_options(futari PRIVATE
    -fno-strict-aliasing
    $<$<CONFIG:Debug>:-O0 -g>
    $<$<CONFIG:Release>:-O3>
)

target_link_libraries(futari PRIVATE rexglue)

# Force-include all generated translation units so hook registration works
if(MSVC)
    target_link_options(futari PRIVATE /WHOLEARCHIVE)
else()
    target_link_options(futari PRIVATE
        -Wl,--whole-archive $<TARGET_FILE:futari> -Wl,--no-whole-archive)
endif()

# ---------------------------------------------------------------------------
# Codegen target — calls ReXGlue to translate the XEX to C++
# ---------------------------------------------------------------------------
find_program(REXGLUE_CODEGEN rexglue-codegen
    HINTS "${REXGLUE_SDK_DIR}/bin" "${REXGLUE_SDK_DIR}/tools")

if(REXGLUE_CODEGEN)
    add_custom_target(codegen
        COMMAND "${REXGLUE_CODEGEN}"
                "${CMAKE_CURRENT_SOURCE_DIR}/../config/futari_manifest.toml"
                "${REXGLUE_SDK_DIR}/include/rex/ppc_context.h"
        COMMENT "Running ReXGlue codegen for Mushihimesama Futari"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )
else()
    message(WARNING
        "rexglue-codegen not found — build the ReXGlue SDK first, then re-run CMake.\n"
        "  cd thirdparty/rexglue-sdk && cmake --preset ... && cmake --build ...")
endif()
