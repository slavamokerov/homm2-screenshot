# fh2poster — the poster rendering library.
#
# This file is designed to be included both from the homm2-screenshot
# project (add_subdirectory) and from the fheroes2_save_editor project
# (optional "Export poster" integration). It declares only the fh2poster
# target; the fh2core and fh2resources targets must already exist in the
# including project.

find_package(Qt6 REQUIRED COMPONENTS Gui)

add_library(fh2poster STATIC
    ${CMAKE_CURRENT_LIST_DIR}/poster.cpp
    ${CMAKE_CURRENT_LIST_DIR}/maprender.cpp
    ${CMAKE_CURRENT_LIST_DIR}/castlerender.cpp
    ${CMAKE_CURRENT_LIST_DIR}/herocard.cpp
    ${CMAKE_CURRENT_LIST_DIR}/infochips.cpp
    ${CMAKE_CURRENT_LIST_DIR}/poster_quantize.cpp
    ${CMAKE_CURRENT_LIST_DIR}/layout/layout_engine.cpp
    ${CMAKE_CURRENT_LIST_DIR}/layout/layout_presets.cpp
    ${CMAKE_CURRENT_LIST_DIR}/layout_json.cpp
)
target_include_directories(fh2poster PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/layout
)
target_link_libraries(fh2poster PUBLIC fh2core fh2resources)
