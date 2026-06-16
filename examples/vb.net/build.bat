@echo off
setlocal enabledelayedexpansion

:: ============================================
:: Tolk VB.NET Example Build Script
:: ============================================

echo Building VB.NET example...
vbc /nologo /reference:TolkDotNet.dll ConsoleApp.vb
if %errorlevel% neq 0 (
    echo ERROR: Build failed.
    exit /b 1
)

echo Build successful: ConsoleApp.exe
endlocal
