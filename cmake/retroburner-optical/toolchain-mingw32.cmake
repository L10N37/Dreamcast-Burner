# RetroBurner modern MinGW32 toolchain
# Owned by RetroBurner; unrelated to Schilling RULES/SMakefile.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_C_COMPILER "
C:/msys64/mingw32/bin/gcc.exe
")
set(CMAKE_RC_COMPILER "
C:/msys64/mingw32/bin/windres.exe
")
set(CMAKE_AR "
C:/msys64/mingw32/bin/ar.exe
")
set(CMAKE_RANLIB "
C:/msys64/mingw32/bin/ranlib.exe
")

# Keep CMake's compiler probes simple while the legacy engine is migrated.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

