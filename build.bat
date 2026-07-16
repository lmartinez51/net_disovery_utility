:: build.bat — Convenience build script for NetDiscovery (Phase 1)
::
:: Run this from a Visual Studio "x64 Native Tools Command Prompt"
:: (found in the Start menu under your Visual Studio installation).
::
:: Usage:
::   build.bat [Release|Debug]
::
:: Output:
::   build\bin\<config>\NetDiscovery.exe

@echo off
setlocal

set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

echo ============================================================
echo  NetDiscovery — Build Script
echo  Configuration: %CONFIG%
echo ============================================================

:: Configure
cmake -B build -S . -G "Ninja" -DCMAKE_BUILD_TYPE=%CONFIG% ^
      -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

:: Build
cmake --build build --config %CONFIG%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    exit /b %ERRORLEVEL%
)

echo.
echo ============================================================
echo  Build succeeded.
echo  Executable: build\bin\NetDiscovery.exe
echo ============================================================
echo.

endlocal
