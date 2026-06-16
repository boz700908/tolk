@echo off
setlocal enabledelayedexpansion

:: ============================================
:: Tolk C# Example Build Script
:: ============================================

echo Building C# example...
csc /nologo /reference:TolkDotNet.dll ConsoleApp.cs
if %errorlevel% neq 0 (
    echo ERROR: Build failed.
    exit /b 1
)

echo Build successful: ConsoleApp.exe
endlocal
