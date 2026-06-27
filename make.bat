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

set clean=0
set build=0
set run=0
set debug=0

if "%~1"=="" (
    set build=1
)

if "%~1"=="full" (
    set clean=1
    set build=1
    set run=1
)

if "%~1"=="clean" (
    set clean=1
)

if "%~1"=="run" (
    set build=1
    set run=1
)

if "%~1"=="build" (
    set clean=1
    set build=1
)

if "%~1"=="debug" (
    set clean=1
    set build=1
    set debug=1
)


if %clean%==1 (
    echo Cleaning...
    if exist %OUTPUT% del %OUTPUT%
    if exist *.pdb del *.pdb
    if exist *.obj del *.obj
    if exist *.ilk del *.ilk
    if exist *.rdi del *.rdi
    echo Done Cleaning...
)

if %build%==1 (
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
)

if %debug%==1 (
    echo Opening debugger...
    start raddbg.exe run.exe
)

if %run%==1 (
    echo Launching program...
    run.exe
)

