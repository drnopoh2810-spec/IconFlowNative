cmake_minimum_required(VERSION 3.20)
project(IconFlowNative VERSION 1.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── AE SDK root ───────────────────────────────────────────────────────────────
if(NOT DEFINED ENV{AE_SDK_ROOT} AND NOT AE_SDK_ROOT)
    message(FATAL_ERROR
        "AE_SDK_ROOT is not set.\n"
        "Set it as an environment variable or pass -DAE_SDK_ROOT=<path> to CMake.\n"
        "Example: cmake -B build -DAE_SDK_ROOT=\"C:/Adobe/AfterEffectsSDK/2024\"")
endif()

if(AE_SDK_ROOT)
    set(SDK_ROOT "${AE_SDK_ROOT}")
else()
    set(SDK_ROOT "$ENV{AE_SDK_ROOT}")
endif()

# Validate SDK presence.
if(NOT EXISTS "${SDK_ROOT}/Headers/AE_Effect.h")
    message(FATAL_ERROR
        "AE SDK not found at '${SDK_ROOT}'.\n"
        "Expected: ${SDK_ROOT}/Headers/AE_Effect.h\n"
        "Download the AE SDK from Adobe Developer Console and set AE_SDK_ROOT.")
endif()

message(STATUS "Using AE SDK at: ${SDK_ROOT}")

# ── Include paths ─────────────────────────────────────────────────────────────
set(AE_INCLUDES
    "${SDK_ROOT}/Headers"
    "${SDK_ROOT}/Headers/SP"
    "${SDK_ROOT}/Resources"
    "${SDK_ROOT}/Examples/Util"
)

# ── Source files ──────────────────────────────────────────────────────────────
set(PLUGIN_SOURCES
    src/IconFlowNative.cpp
    src/Renderer.cpp
)

set(PLUGIN_HEADERS
    src/IconFlowNative.h
    src/ColorMath.h
    src/Renderer.h
)

# ── Resource file (PiPL + version info) ──────────────────────────────────────
set(PLUGIN_RC src/IconFlowNative.rc)

# ── Plugin target ─────────────────────────────────────────────────────────────
add_library(IconFlowNative SHARED
    ${PLUGIN_SOURCES}
    ${PLUGIN_HEADERS}
    ${PLUGIN_RC}
)

target_include_directories(IconFlowNative PRIVATE
    ${AE_INCLUDES}
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# ── Windows-specific settings ─────────────────────────────────────────────────
if(WIN32)
    # Output as .aex (AE effect extension) instead of .dll.
    set_target_properties(IconFlowNative PROPERTIES
        OUTPUT_NAME   "IconFlowNative"
        SUFFIX        ".aex"
        PREFIX        ""
    )

    target_compile_definitions(IconFlowNative PRIVATE
        WIN32
        _WINDOWS
        MSWindows
        _CRT_SECURE_NO_WARNINGS
    )

    target_compile_options(IconFlowNative PRIVATE
        /W3         # Warning level 3
        /WX-        # Don't treat warnings as errors (SDK headers may warn)
        /MP         # Multi-processor compilation
        /Zi         # Debug info (even in Release for crash analysis)
        /O2         # Optimize for speed
        /Oi         # Enable intrinsics
        /GL         # Whole program optimization
        /EHsc       # C++ exceptions
    )

    target_link_options(IconFlowNative PRIVATE
        /LTCG                    # Link-time code generation
        /SUBSYSTEM:WINDOWS
        /DLL
        /MACHINE:X64
        /NODEFAULTLIB:LIBCMT     # AE plugins use the DLL CRT
    )

    target_link_libraries(IconFlowNative PRIVATE
        kernel32
        user32
        shell32
        gdi32
    )

    # Export entry point by name (no decorated name mangling).
    set_target_properties(IconFlowNative PROPERTIES
        LINK_FLAGS "/EXPORT:EffectMain"
    )
endif()

# ── PiPL pre-build step ───────────────────────────────────────────────────────
# Converts pipl/IconFlowNative.r → src/IconFlowNative.rr using AE PiPLtool.
# The .rr is #included by IconFlowNative.rc.
set(PIPLTOOL "${SDK_ROOT}/Resources/PiPLtool.exe")

if(EXISTS "${PIPLTOOL}")
    add_custom_command(
        OUTPUT  "${CMAKE_CURRENT_SOURCE_DIR}/src/IconFlowNative.rr"
        COMMAND "${PIPLTOOL}"
                "${CMAKE_CURRENT_SOURCE_DIR}/pipl/IconFlowNative.r"
                "${CMAKE_CURRENT_SOURCE_DIR}/src/IconFlowNative.rr"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/pipl/IconFlowNative.r"
        COMMENT "Compiling PiPL resource with PiPLtool"
        VERBATIM
    )
    add_custom_target(PiPLGenerate
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/IconFlowNative.rr"
    )
    add_dependencies(IconFlowNative PiPLGenerate)
else()
    message(WARNING
        "PiPLtool not found at '${PIPLTOOL}'.\n"
        "Run configure_windows.ps1 to generate src/IconFlowNative.rr manually,\n"
        "or ensure the AE SDK is fully installed.")
endif()

# ── Unit tests (no AE SDK dependency) ────────────────────────────────────────
option(BUILD_TESTS "Build unit tests for color math functions" ON)

if(BUILD_TESTS)
    add_executable(ColorMathTests
        tests/ColorMathTests.cpp
    )
    target_include_directories(ColorMathTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_features(ColorMathTests PRIVATE cxx_std_17)

    enable_testing()
    add_test(NAME ColorMath COMMAND ColorMathTests)

    message(STATUS "Unit tests enabled. Run: ctest --output-on-failure")
endif()

# ── Installation ──────────────────────────────────────────────────────────────
# AE effects go in: %PROGRAMFILES%\Adobe\Adobe After Effects 2024\Support Files\Plug-ins\
# or per-user: %APPDATA%\Adobe\After Effects\(version)\Plug-ins\
install(TARGETS IconFlowNative
    RUNTIME DESTINATION "."
    LIBRARY DESTINATION "."
)
