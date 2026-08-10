cmake_minimum_required(VERSION 3.24)

project(DreamcastBurner VERSION 0.3.0 LANGUAGES C CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "Dreamcast Burner is a native Windows application. Configure it on Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# /MT and /MTd keep the finished program independent of the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
set(CDIRIP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/cdirip")
if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
    message(FATAL_ERROR
        "Dear ImGui is missing. Run scripts/bootstrap-deps.ps1 before configuring.")
endif()

add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
)

target_include_directories(imgui PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)

target_compile_definitions(imgui PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

add_executable(cdirip
    "${CDIRIP_DIR}/audio.c"
    "${CDIRIP_DIR}/buffer.c"
    "${CDIRIP_DIR}/cdi.c"
    "${CDIRIP_DIR}/cdirip.c"
    "${CDIRIP_DIR}/common.c"
)

target_include_directories(cdirip PRIVATE "${CDIRIP_DIR}")
target_compile_definitions(cdirip PRIVATE
    _CRT_SECURE_NO_WARNINGS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_link_libraries(cdirip PRIVATE comdlg32 shell32 ole32)
if(MSVC)
    target_compile_options(cdirip PRIVATE /W3 /utf-8)
endif()

add_executable(DreamcastBurner WIN32
    src/main.cpp
    src/burn_engine.cpp
    src/burn_engine.h
    src/drive_manager.cpp
    src/drive_manager.h
    src/texture_loader.cpp
    src/texture_loader.h
    src/resource.h
    src/DreamcastBurner.rc
)

target_compile_definitions(DreamcastBurner PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(DreamcastBurner PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
        /Zc:__cplusplus
    )
    # DreamcastBurner.rc already embeds the requested-execution and DPI manifest.
    target_link_options(DreamcastBurner PRIVATE /MANIFEST:NO)
endif()

target_link_libraries(DreamcastBurner PRIVATE
    imgui
    d3d11
    dxgi
    windowscodecs
    ole32
    shell32
)

add_dependencies(DreamcastBurner cdirip)
add_custom_command(TARGET DreamcastBurner POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:DreamcastBurner>/tools"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:DreamcastBurner>/licenses"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:cdirip>"
        "$<TARGET_FILE_DIR:DreamcastBurner>/tools/cdirip.exe"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/cdrecord.exe"
        "$<TARGET_FILE_DIR:DreamcastBurner>/tools/cdrecord.exe"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/cygwin1.dll"
        "$<TARGET_FILE_DIR:DreamcastBurner>/tools/cygwin1.dll"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CDIRIP_DIR}/LICENSE"
        "$<TARGET_FILE_DIR:DreamcastBurner>/licenses/CDIrip-GPL-2.0.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/licenses/cdrecord-cddl.txt"
        "$<TARGET_FILE_DIR:DreamcastBurner>/licenses/cdrecord-CDDL.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/licenses/cdrecord-gpl3.txt"
        "$<TARGET_FILE_DIR:DreamcastBurner>/licenses/cdrecord-GPL.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/licenses/cygwin-GPL-2.0.txt"
        "$<TARGET_FILE_DIR:DreamcastBurner>/licenses/Cygwin-GPL-2.0.txt"
    VERBATIM
)

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT DreamcastBurner)

install(TARGETS DreamcastBurner RUNTIME DESTINATION .)
install(TARGETS cdirip RUNTIME DESTINATION tools)
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/cdrecord.exe"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/cygwin1.dll"
    DESTINATION tools)
install(FILES "${IMGUI_DIR}/LICENSE.txt"
    DESTINATION licenses
    RENAME DearImGui-LICENSE.txt)
install(FILES "${CDIRIP_DIR}/LICENSE"
    DESTINATION licenses
    RENAME CDIrip-GPL-2.0.txt)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/licenses/cdrecord-cddl.txt"
    DESTINATION licenses
    RENAME cdrecord-CDDL.txt)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/licenses/cdrecord-gpl3.txt"
    DESTINATION licenses
    RENAME cdrecord-GPL.txt)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/licenses/cygwin-GPL-2.0.txt"
    DESTINATION licenses
    RENAME Cygwin-GPL-2.0.txt)
