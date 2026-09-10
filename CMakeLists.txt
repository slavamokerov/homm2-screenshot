cmake_minimum_required(VERSION 3.21)
project(homm2-screenshot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(QT_NO_KEYWORDS)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui)
find_package(ZLIB REQUIRED)

# === Shared core (fh2core + fh2resources) ===
#
# The core lives in the fheroes2-save-editor repository (see AGENTS.md).
# Connection modes (FH2_CORE_MODE):
#   auto (default): vendored copy vendor/fh2core/ when present, live otherwise;
#   vendor: the frozen snapshot synced by scripts/sync-core.sh — builds without
#           the editor repository;
#   live: compiles the core straight from a local editor checkout
#         (-DFH2_EDITOR_SRC=...), development mode.
# The default editor path below is the sibling folder fheroes2-save-editor.
set(FH2_CORE_MODE "auto" CACHE STRING "Core connection mode: auto | vendor | live")
set_property(CACHE FH2_CORE_MODE PROPERTY STRINGS auto vendor live)

set(FH2_VENDOR_DIR "${CMAKE_CURRENT_LIST_DIR}/vendor/fh2core")
set(FH2_EDITOR_SRC "${CMAKE_CURRENT_LIST_DIR}/../fheroes2-save-editor/src"
    CACHE PATH "Path to the fheroes2-save-editor sources (live core mode)")

set(FH2_CORE_DIR "")
if(FH2_CORE_MODE STREQUAL "auto")
    if(EXISTS "${FH2_VENDOR_DIR}/savefile.cpp")
        set(FH2_CORE_MODE "vendor")
    else()
        set(FH2_CORE_MODE "live")
    endif()
endif()

if(FH2_CORE_MODE STREQUAL "vendor")
    if(NOT EXISTS "${FH2_VENDOR_DIR}/savefile.cpp")
        message(FATAL_ERROR "FH2_CORE_MODE=vendor but ${FH2_VENDOR_DIR} is missing; run scripts/sync-core.sh <path-to-fheroes2-save-editor>")
    endif()
    set(FH2_CORE_DIR "${FH2_VENDOR_DIR}")
    message(STATUS "fh2core: vendored copy (${FH2_CORE_DIR}); run scripts/sync-core.sh to update")
elseif(FH2_CORE_MODE STREQUAL "live")
    if(NOT EXISTS "${FH2_EDITOR_SRC}/savefile.cpp")
        message(FATAL_ERROR "FH2_CORE_MODE=live but no editor checkout at ${FH2_EDITOR_SRC}; set -DFH2_EDITOR_SRC=<path-to-editor-src>")
    endif()
    set(FH2_CORE_DIR "${FH2_EDITOR_SRC}")
    message(STATUS "fh2core: live sources from ${FH2_CORE_DIR}")
else()
    message(FATAL_ERROR "Unknown FH2_CORE_MODE '${FH2_CORE_MODE}'; use auto | vendor | live")
endif()

# Save parsing core (no Qt).
add_library(fh2core STATIC
    ${FH2_CORE_DIR}/constants.cpp
    ${FH2_CORE_DIR}/gettextmo.cpp
    ${FH2_CORE_DIR}/savefile.cpp
    ${FH2_CORE_DIR}/worldparse.cpp
)
target_include_directories(fh2core PUBLIC ${FH2_CORE_DIR})
target_link_libraries(fh2core PUBLIC ZLIB::ZLIB)

# Game resources (AGG/ICN/TIL/KB.PAL) and the game font (Qt6::Gui only).
add_library(fh2resources STATIC
    ${FH2_CORE_DIR}/aggicn.cpp
    ${FH2_CORE_DIR}/assets.cpp
    ${FH2_CORE_DIR}/gamefont.cpp
)
target_include_directories(fh2resources PUBLIC ${FH2_CORE_DIR})
target_link_libraries(fh2resources PUBLIC Qt6::Gui)

# === homm2-to-fheroes2 core (h2core) ===
#
# Parses the ORIGINAL Heroes of Might and Magic II saves (.GM1/.GMC/.GXC) and
# converts them into fheroes2 .sav bytes (parseSave -> convert -> buildSaveFile),
# so the poster can consume HoMM2 saves directly. Lives in the homm2-to-fheroes2
# repository. Connection modes (H2_CORE_MODE): auto | vendor | live — same idea
# as FH2_CORE_MODE above. The core is Qt-free. It defines its OWN fh2::WorldData
# (the converter writer model), which collides with fh2core's parsed model, so it
# must only be included from the isolated bridge (src/homm2_bridge.cpp).
set(H2_CORE_MODE "auto" CACHE STRING "h2core connection mode: auto | vendor | live")
set_property(CACHE H2_CORE_MODE PROPERTY STRINGS auto vendor live)

set(H2_VENDOR_DIR "${CMAKE_CURRENT_LIST_DIR}/vendor/h2core")
set(H2_SRC "${CMAKE_CURRENT_LIST_DIR}/../homm2-to-fheroes2/src"
    CACHE PATH "Path to the homm2-to-fheroes2 sources (live h2core mode)")

set(H2_CORE_DIR "")
if(H2_CORE_MODE STREQUAL "auto")
    if(EXISTS "${H2_VENDOR_DIR}/convert.cpp")
        set(H2_CORE_MODE "vendor")
    else()
        set(H2_CORE_MODE "live")
    endif()
endif()

if(H2_CORE_MODE STREQUAL "vendor")
    if(NOT EXISTS "${H2_VENDOR_DIR}/convert.cpp")
        message(FATAL_ERROR "H2_CORE_MODE=vendor but ${H2_VENDOR_DIR} is missing; run scripts/sync-h2core.sh")
    endif()
    set(H2_CORE_DIR "${H2_VENDOR_DIR}")
    message(STATUS "h2core: vendored copy (${H2_CORE_DIR}); run scripts/sync-h2core.sh to update")
elseif(H2_CORE_MODE STREQUAL "live")
    if(NOT EXISTS "${H2_SRC}/convert.cpp")
        message(FATAL_ERROR "H2_CORE_MODE=live but no homm2-to-fheroes2 checkout at ${H2_SRC}; set -DH2_SRC=<path-to-homm2-to-fheroes2-src>")
    endif()
    set(H2_CORE_DIR "${H2_SRC}")
    message(STATUS "h2core: live sources from ${H2_CORE_DIR}")
else()
    message(FATAL_ERROR "Unknown H2_CORE_MODE '${H2_CORE_MODE}'; use auto | vendor | live")
endif()

# HoMM2 converter core (no Qt, no fh2core).
add_library(h2core STATIC
    ${H2_CORE_DIR}/homm2_save.cpp
    ${H2_CORE_DIR}/fheroes2_save.cpp
    ${H2_CORE_DIR}/convert.cpp
)
target_include_directories(h2core PUBLIC ${H2_CORE_DIR})
target_link_libraries(h2core PUBLIC ZLIB::ZLIB)

# Poster library (layout + map/castle/hero/chip rendering) — consumed by this
# CLI and, optionally, by the fheroes2 save editor ("Export poster").
add_subdirectory(libs/fh2poster)

# === rastercompat — headless software Qt-compat (wasm builds) ===
# Implements a subset of QtGui/QtCore pixel ops with the rastercompat.hpp header
# se so the shared render code compiles under Emscripten without Qt for WASM.
# Only consumed by the WASM build and the parity test.
add_library(rastercompat STATIC
    libs/rastercompat/rastercompat.cpp
)
target_include_directories(rastercompat PUBLIC ${CMAKE_CURRENT_LIST_DIR}/libs/rastercompat/include)
target_link_libraries(rastercompat PUBLIC ZLIB::ZLIB)

add_executable(homm2-screenshot
    src/main.cpp
    src/args.cpp
    src/homm2_bridge.cpp
)
target_link_libraries(homm2-screenshot PRIVATE fh2poster h2core)

include(CTest)
if(BUILD_TESTING)
    add_executable(layout_test
        tests/layout_test.cpp
        libs/fh2poster/layout/layout_engine.cpp
        libs/fh2poster/layout/layout_presets.cpp
    )
    target_include_directories(layout_test PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/libs/fh2poster ${CMAKE_CURRENT_LIST_DIR}/libs/fh2poster/layout)
    add_test(NAME layout_test COMMAND layout_test)

    add_executable(world_test
        tests/world_test.cpp
    )
    target_link_libraries(world_test PRIVATE fh2core)
    # Needs real saves: -DFH2_TEST_SAVES="path1.sav;path2.sav"
    if(FH2_TEST_SAVES)
        add_test(NAME world_test COMMAND world_test ${FH2_TEST_SAVES})
    endif()

    add_executable(json_test
        tests/json_test.cpp
    )
    target_link_libraries(json_test PRIVATE fh2poster)
    add_test(NAME json_test COMMAND json_test)

    # HoMM2 save -> fheroes2 .sav bytes round-trip (needs homm2-to-fheroes2 core).
    add_executable(homm2_test
        tests/homm2_test.cpp
        src/homm2_bridge.cpp
    )
    target_include_directories(homm2_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
    target_link_libraries(homm2_test PRIVATE h2core fh2core)
    # Needs an original .GM1 fixture: -DH2_TEST_FIXTURES="dir;path.gm1"
    if(H2_TEST_FIXTURES)
        add_test(NAME homm2_test COMMAND homm2_test ${H2_TEST_FIXTURES})
    endif()

    # Renderer parity: draw the same scene via real Qt (CLI reference) and via
    # the rastercompat mini-Qt (WASM build) — their RGBA hashes must match.
    add_executable(parity_qt
        tools/parity_qt.cpp
    )
    target_include_directories(parity_qt PRIVATE ${CMAKE_CURRENT_LIST_DIR}/tools)
    target_link_libraries(parity_qt PRIVATE Qt6::Gui Qt6::Core)

    add_executable(parity_soft
        tools/parity_soft.cpp
    )
    target_include_directories(parity_soft PRIVATE ${CMAKE_CURRENT_LIST_DIR}/tools)
    target_link_libraries(parity_soft PRIVATE rastercompat)
    add_test(NAME parity_qt COMMAND parity_qt)
    add_test(NAME parity_soft COMMAND parity_soft)
endif()
