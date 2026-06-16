@echo off
setlocal enabledelayedexpansion

:: ============================================
:: Tolk AutoIt Example Build Script
:: ============================================

echo Building AutoIt example...
Aut2exe /in ConsoleApp.au3 /pack /unicode /console
if %errorlevel% neq 0 (
    echo ERROR: AutoIt compilation failed.
    exit /b 1
)

echo Build successful: ConsoleApp.exe
endlocal
