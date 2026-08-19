cmake_minimum_required(VERSION 3.24)

project(RetroBurner VERSION 0.4.0 LANGUAGES C CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "Retro Burner is a native Windows application. Configure it on Windows.")
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
    src/burnermax.cpp
    src/burnermax.h
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
        "${CDIRIP_EMBED_DIR}/cdirip.exe;${CMAKE_CURRENT_SOURCE_DIR}/tools/retrobeam-dev/retrobeam.exe;${CMAKE_CURRENT_SOURCE_DIR}/tools/growisofs.exe;${CMAKE_CURRENT_SOURCE_DIR}/tools/dvd+rw-mediainfo.exe;${CMAKE_CURRENT_SOURCE_DIR}/tools/abgx360.exe;${CMAKE_CURRENT_SOURCE_DIR}/assets/BurningXB360.png;${CMAKE_CURRENT_SOURCE_DIR}/assets/mixkit-cartoon-monkey-mocking-laugh-107.wav;${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY.md;${CDIRIP_DIR}/LICENSE;${CMAKE_CURRENT_SOURCE_DIR}/licenses/cdrecord-cddl.txt;${CMAKE_CURRENT_SOURCE_DIR}/licenses/cdrecord-gpl3.txt;${CMAKE_CURRENT_SOURCE_DIR}/licenses/cygwin-GPL-2.0.txt;${CMAKE_CURRENT_SOURCE_DIR}/licenses/dvd-rw-tools-GPL-2.0.txt;${CMAKE_CURRENT_SOURCE_DIR}/licenses/ABGX360-GPL-2.0.txt;${CMAKE_CURRENT_SOURCE_DIR}/licenses/Mixkit-Sound-Effects-NOTICE.txt;${CMAKE_CURRENT_SOURCE_DIR}/licenses/BurnerMAX-NOTICE.txt;${IMGUI_DIR}/LICENSE.txt"
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
/WX
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
# RB_BURNERMAX_READONLY_LAB_35
# Read-only vendor-register research utility. This target is deliberately
# separate from RetroBurner and contains no vendor-register write path.
add_executable(RetroBurnerBurnerMaxProbe EXCLUDE_FROM_ALL
    src/burnermax_probe_cli.cpp
)

target_compile_definitions(RetroBurnerBurnerMaxProbe PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(RetroBurnerBurnerMaxProbe PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
        /Zc:__cplusplus
    )
endif()

set_target_properties(RetroBurnerBurnerMaxProbe PROPERTIES
    OUTPUT_NAME "retroburner-burnermax-probe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
)
# RB_ASUS_VENDOR_FAMILY_PROBE_36
# Finite DATA-IN-only command-family probe based on published QPxTool
# ASUS/Lite-On query commands. No vendor write path exists in this target.
add_executable(RetroBurnerAsusFamilyProbe EXCLUDE_FROM_ALL
    src/asus_vendor_family_probe_cli.cpp
)

target_compile_definitions(RetroBurnerAsusFamilyProbe PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(RetroBurnerAsusFamilyProbe PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
        /Zc:__cplusplus
    )
endif()

set_target_properties(RetroBurnerAsusFamilyProbe PROPERTIES
    OUTPUT_NAME "retroburner-asus-family-probe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
)
# RB_ASUS_SHARED_BUFFER_PROBE_79
# Read-only discriminator: tests whether DF/02/09 exposes a shared firmware
# response buffer by bracketing it with standard MMC DATA-IN queries.
add_executable(RetroBurnerAsusSharedBufferProbe EXCLUDE_FROM_ALL
    src/asus_shared_buffer_probe_cli.cpp
)

target_compile_definitions(RetroBurnerAsusSharedBufferProbe PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(RetroBurnerAsusSharedBufferProbe PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
        /Zc:__cplusplus
    )
endif()

set_target_properties(RetroBurnerAsusSharedBufferProbe PROPERTIES
    OUTPUT_NAME "retroburner-asus-shared-buffer-probe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
)
# RB_LTNFLASH_READ_PROBE_81
# Exact DATA-IN-only vendor reads recovered statically from
# LtnFlash 2.6.11 / embedded MFlashPI.class.
add_executable(RetroBurnerLtnFlashReadProbe EXCLUDE_FROM_ALL
    src/ltnflash_read_probe_cli.cpp
)

target_compile_definitions(RetroBurnerLtnFlashReadProbe PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(RetroBurnerLtnFlashReadProbe PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
        /Zc:__cplusplus
    )
endif()

set_target_properties(RetroBurnerLtnFlashReadProbe PROPERTIES
    OUTPUT_NAME "retroburner-ltnflash-read-probe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
)
# RB_LTNFLASH_CHALLENGE_PROBE_82
# DATA-IN-only PLDS/LtnFlash registration challenge discriminator.
add_executable(RetroBurnerLtnFlashChallengeProbe EXCLUDE_FROM_ALL
    src/ltnflash_challenge_probe_cli.cpp
)

target_compile_definitions(RetroBurnerLtnFlashChallengeProbe PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(RetroBurnerLtnFlashChallengeProbe PRIVATE
        /W4
        /permissive-
        /utf-8
        /EHsc
        /Zc:__cplusplus
    )
endif()

set_target_properties(RetroBurnerLtnFlashChallengeProbe PROPERTIES
    OUTPUT_NAME "retroburner-ltnflash-challenge-probe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/tools/burnermax-dev"
)
# RB_STRICT_MSVC_WARNINGS_V84R
# Project-owned C++ and bundled cdirip must compile cleanly.
if(MSVC)
    target_compile_options(RetroBurner PRIVATE
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
        /utf-8
    )

    target_compile_options(cdirip PRIVATE
        /W4
        /WX
    )
endif()