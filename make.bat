@echo off
REM MSVC build for Windows with debugging info

setlocal enabledelayedexpansion

set CC=cl.exe
set CFLAGS=/Zi /Od /std:c++17 /openmp /DGLFW_STATIC /nologo
set INCLUDES=/I include /I include/glad
set LIBPATHS=/LIBPATH:lib
set LIBS=glfw3_mt.lib opengl32.lib gdi32.lib user32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib legacy_stdio_definitions.lib
set SOURCES=src\main.cpp src\glad.c
set OUTPUT=run.exe

if "%1"=="clean" (
    echo Cleaning...
    if exist %OUTPUT% del %OUTPUT%
    if exist *.pdb del *.pdb
    if exist *.obj del *.obj
    if exist *.ilk del *.ilk
    if exist *.rdi del *.rdi
    echo Done.
    exit /b 0
)



echo Building...
%CC% %CFLAGS% %INCLUDES% %SOURCES% /link %LIBPATHS% %LIBS% /OUT:%OUTPUT% /DEBUG:FULL /INCREMENTAL:NO /PDB:run.pdb

if !errorlevel! equ 0 (
    if exist *.obj del *.obj
    if exist *.ilk del *.ilk
    echo Build successful: %OUTPUT%
) else (
    echo Build failed with error code !errorlevel!
    exit /b !errorlevel!
)

if "%1" == "debug" (
    echo Opening debugger...
    start raddbg.exe run.exe
    exit /b 0
)

if "%1"=="run" (
    run.exe
)

