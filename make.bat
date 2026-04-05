@echo off
REM Calls g++.exe directly from MinGW
REM @TODO: For Windows we should probably use MSVC.
REM I started out a bit frustrated with build patterns for C and C++ on Windows, but I need to get over it.

setlocal enabledelayedexpansion

set CC=g++.exe
set CXXFLAGS=-g -std=c++17 -fopenmp
set INCLUDES=-I.\include
set LIBS=-L.\lib -lglfw3dll
set SOURCES=.\src\main.cpp .\src\glad.c
set OUTPUT=.\run.exe

if "%1"=="clean" (
    echo Cleaning...
    if exist %OUTPUT% del %OUTPUT%
    echo Done.
    exit /b 0
)

echo Building...
"%CC%" %CXXFLAGS% %INCLUDES% %SOURCES% %LIBS% -o %OUTPUT%

if !errorlevel! equ 0 (
    echo Build successful: %OUTPUT%
) else (
    echo Build failed with error code !errorlevel!
    exit /b !errorlevel!
)
