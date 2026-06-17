@echo off
setlocal enabledelayedexpansion

:: ============================================
:: Tolk C Example Build Script
:: ============================================

echo Building C example...
cl /nologo Tolk.lib ConsoleApp.c
if %errorlevel% neq 0 (
    echo ERROR: Build failed.
    exit /b 1
)

:: Cleanup temporary files
if exist ConsoleApp.obj del ConsoleApp.obj
echo Build successful: ConsoleApp.exe
endlocal
