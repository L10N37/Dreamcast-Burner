cmake_minimum_required(VERSION 3.24)

project(RetroBurner VERSION 0.4.0 LANGUAGES C CXX RC)

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

set_target_properties(imgui PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/lib"
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


set(CDIRIP_EMBED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools/.embedded")
file(MAKE_DIRECTORY "${CDIRIP_EMBED_DIR}")
set_target_properties(cdirip PROPERTIES
    OUTPUT_NAME "cdirip"
    RUNTIME_OUTPUT_DIRECTORY "${CDIRIP_EMBED_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CDIRIP_EMBED_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CDIRIP_EMBED_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CDIRIP_EMBED_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CDIRIP_EMBED_DIR}"
)
add_executable(RetroBurner WIN32
    src/main.cpp
    src/burn_engine.cpp
    src/burn_engine.h
    src/drive_manager.cpp
    src/drive_manager.h
    src/embedded_tools.cpp
    src/embedded_tools.h
    src/texture_loader.cpp
    src/texture_loader.h
    src/resource.h
    src/RetroBurner.rc
)


set_source_files_properties(src/RetroBurner.rc PROPERTIES
    OBJECT_DEPENDS
        "${CDIRIP_EMBED_DIR}/cdirip.exe;${CMAKE_CURRENT_SOURCE_DIR}/tools/cdrecord.exe;${CMAKE_CURRENT_SOURCE_DIR}/tools/cygwin1.dll;${CMAKE_CURRENT_SOURCE_DIR}/tools/growisofs.exe;${CMAKE_CURRENT_SOURCE_DIR}/tools/dvd+rw-mediainfo.exe;${CMAKE_CURRENT_SOURCE_DIR}/assets/mixkit-cartoon-monkey-mocking-laugh-107.wav;${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY.md;${CDIRIP_DIR}/LICENSE;${CMAKE_CURRENT_SOURCE_DIR}/licenses/cdrecord-cddl.txt;${CMAKE_CURRENT_SOURCE_DIR}/licenses/cdrecord-gpl3.txt;${CMAKE_CURRENT_SOURCE_DIR}/licenses/cygwin-GPL-2.0.txt;${IMGUI_DIR}/LICENSE.txt"
)
target_compile_definitions(RetroBurner PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(RetroBurner PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
        /Zc:__cplusplus
    )
    # RetroBurner.rc already embeds the requested-execution and DPI manifest.
    target_link_options(RetroBurner PRIVATE /MANIFEST:NO)
endif()

target_link_libraries(RetroBurner PRIVATE
    imgui
    d3d11
    dxgi
    windowscodecs
    ole32
    shell32
    winmm
)

add_dependencies(RetroBurner cdirip)
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT RetroBurner)

install(TARGETS RetroBurner RUNTIME DESTINATION .)